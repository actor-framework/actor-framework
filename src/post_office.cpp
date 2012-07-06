/******************************************************************************\
 *           ___        __                                                    *
 *          /\_ \    __/\ \                                                   *
 *          \//\ \  /\_\ \ \____    ___   _____   _____      __               *
 *            \ \ \ \/\ \ \ '__`\  /'___\/\ '__`\/\ '__`\  /'__`\             *
 *             \_\ \_\ \ \ \ \L\ \/\ \__/\ \ \L\ \ \ \L\ \/\ \L\.\_           *
 *             /\____\\ \_\ \_,__/\ \____\\ \ ,__/\ \ ,__/\ \__/.\_\          *
 *             \/____/ \/_/\/___/  \/____/ \ \ \/  \ \ \/  \/__/\/_/          *
 *                                          \ \_\   \ \_\                     *
 *                                           \/_/    \/_/                     *
 *                                                                            *
 * Copyright (C) 2011, 2012                                                   *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation, either version 3 of the License                  *
 * or (at your option) any later version.                                     *
 *                                                                            *
 * libcppa is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                       *
 * See the GNU Lesser General Public License for more details.                *
 *                                                                            *
 * You should have received a copy of the GNU Lesser General Public License   *
 * along with libcppa. If not, see <http://www.gnu.org/licenses/>.            *
\******************************************************************************/


#include <new>          // placement new
#include <ios>          // ios_base::failure
#include <list>         // std::list
#include <thread>
#include <vector>       // std::vector
#include <sstream>
#include <cstring>      // strerror
#include <cstdint>      // std::uint32_t, std::uint64_t
#include <iostream>     // std::cout, std::cerr, std::endl
#include <exception>    // std::logic_error
#include <algorithm>    // std::find_if
#include <stdexcept>    // std::underflow_error

#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/tcp.h>

#include "cppa/cppa.hpp"
#include "cppa/atom.hpp"
#include "cppa/match.hpp"
#include "cppa/config.hpp"
#include "cppa/to_string.hpp"
#include "cppa/deserializer.hpp"
#include "cppa/binary_deserializer.hpp"

#include "cppa/detail/buffer.hpp"
#include "cppa/detail/mailman.hpp"
#include "cppa/detail/types_array.hpp"
#include "cppa/detail/post_office.hpp"
#include "cppa/detail/native_socket.hpp"
#include "cppa/detail/actor_registry.hpp"
#include "cppa/detail/network_manager.hpp"
#include "cppa/detail/singleton_manager.hpp"
#include "cppa/detail/actor_proxy_cache.hpp"
#include "cppa/detail/addressed_message.hpp"

#define DEBUG(arg)                                                             \
    std::cout << "[process id: "                                               \
              << cppa::process_information::get()->process_id()                \
              << "] " << arg << std::endl

#undef DEBUG
#define DEBUG(unused) ((void) 0)

using std::cout;
using std::cerr;
using std::endl;

namespace {

cppa::detail::types_array<cppa::atom_value, cppa::actor_ptr> t_atom_actor_ptr_types;

// allocate in 1KB chunks (minimize reallocations)
constexpr size_t s_chunk_size = 1024;

// allow up to 1MB per buffer
constexpr size_t s_max_buffer_size = (1024 * 1024);

static_assert((s_max_buffer_size % s_chunk_size) == 0,
              "max_buffer_size is not a multiple of chunk_size");

static_assert(sizeof(cppa::detail::native_socket_type) == sizeof(std::uint32_t),
              "sizeof(native_socket_t) != sizeof(std::uint32_t)");

} // namespace <anonmyous>

namespace cppa { namespace detail {

inline void send2po_(network_manager*) { }

template<typename Arg0, typename... Args>
inline void send2po_(network_manager* nm, Arg0&& arg0, Args&&... args) {
    nm->send_to_post_office(make_any_tuple(std::forward<Arg0>(arg0),
                                           std::forward<Args>(args)...));
}


template<typename... Args>
inline void send2po(const po_message& msg, Args&&... args) {
    auto nm = singleton_manager::get_network_manager();
    nm->send_to_post_office(msg);
    send2po_(nm, std::forward<Args>(args)...);
}

template<class Fun>
struct scope_guard {
    Fun m_fun;
    scope_guard(Fun&& fun) : m_fun(fun) { }
    ~scope_guard() { m_fun(); }
};

template<class Fun>
scope_guard<Fun> make_scope_guard(Fun fun) { return {std::move(fun)}; }

class po_socket_handler {

 public:

    po_socket_handler(native_socket_type fd) : m_socket(fd) { }

    virtual ~po_socket_handler() { }

    // returns bool if either done or an error occured
    virtual bool read_and_continue() = 0;

    native_socket_type get_socket() const {
        return m_socket;
    }

    virtual bool is_doorman_of(actor_id) const { return false; }

 protected:

    native_socket_type m_socket;

};

typedef std::unique_ptr<po_socket_handler> po_socket_handler_ptr;
typedef std::vector<po_socket_handler_ptr> po_socket_handler_vector;

// represents a TCP connection to another peer
class po_peer : public po_socket_handler {

    enum state {
        // connection just established; waiting for process information
        wait_for_process_info,
        // wait for the size of the next message
        wait_for_msg_size,
        // currently reading a message
        read_message
    };

    state m_state;
    // caches process_information::get()
    process_information_ptr m_pself;
    // the process information of our remote peer
    process_information_ptr m_peer;
    // caches uniform_typeid<addressed_message>()
    const uniform_type_info* m_meta_msg;
    // manages socket input
    buffer<512, (16 * 1024 * 1024)> m_buf;

 public:

    po_peer(native_socket_type fd, process_information_ptr peer = nullptr)
        : po_socket_handler(fd)
        , m_state((peer) ? wait_for_msg_size : wait_for_process_info)
        , m_pself(process_information::get())
        , m_peer(std::move(peer))
        , m_meta_msg(uniform_typeid<addressed_message>()) {
        m_buf.reset(m_state == wait_for_process_info
                    ? sizeof(std::uint32_t) + process_information::node_id_size
                    : sizeof(std::uint32_t));
    }

    ~po_peer() {
        closesocket(m_socket);
        if (m_peer) {
            // collect all children (proxies to actors of m_peer)
            std::vector<actor_proxy_ptr> children;
            children.reserve(20);
            get_actor_proxy_cache().erase_all(m_peer->node_id(),
                                              m_peer->process_id(),
                                              [&](actor_proxy_ptr& pptr) {
                children.push_back(std::move(pptr));
            });
            // kill all proxies
            for (actor_proxy_ptr& pptr: children) {
                pptr->enqueue(nullptr,
                              make_any_tuple(atom("KILL_PROXY"),
                                             exit_reason::remote_link_unreachable));
            }
        }
    }

    inline native_socket_type get_socket() const { return m_socket; }

    // @returns false if an error occured; otherwise true
    bool read_and_continue() {
        for (;;) {
            if (m_buf.append_from(m_socket) == false) {
                DEBUG("cannot read from socket");
                return false;
            }
            if (m_buf.ready() == false) {
                return true; // try again later
            }
            switch (m_state) {
                case wait_for_process_info: {
                    std::uint32_t process_id;
                    process_information::node_id_type node_id;
                    memcpy(&process_id, m_buf.data(), sizeof(std::uint32_t));
                    memcpy(node_id.data(), m_buf.data() + sizeof(std::uint32_t),
                           process_information::node_id_size);
                    m_peer.reset(new process_information(process_id, node_id));
                    // inform mailman about new peer
                    singleton_manager::get_network_manager()
                    ->send_to_mailman(make_any_tuple(m_socket, m_peer));
                    m_state = wait_for_msg_size;
                    m_buf.reset(sizeof(std::uint32_t));
                    DEBUG("pinfo read: "
                          << m_peer->process_id()
                          << "@"
                          << to_string(m_peer->node_id()));
                    break;
                }
                case wait_for_msg_size: {
                    std::uint32_t msg_size;
                    memcpy(&msg_size, m_buf.data(), sizeof(std::uint32_t));
                    m_buf.reset(msg_size);
                    m_state = read_message;
                    break;
                }
                case read_message: {
                    addressed_message msg;
                    binary_deserializer bd(m_buf.data(), m_buf.size());
                    try {
                        m_meta_msg->deserialize(&msg, &bd);
                    }
                    catch (std::exception& e) {
                        // unable to deserialize message (format error)
                        DEBUG(to_uniform_name(typeid(e)) << ": " << e.what());
                        return false;
                    }
                    auto& content = msg.content();
                    DEBUG("<-- " << to_string(msg));
                    match(content) (
                        on(atom("MONITOR")) >> [&]() {
                            auto receiver = msg.receiver().downcast<actor>();
                            CPPA_REQUIRE(receiver.get() != nullptr);
                            if (!receiver) {
                                DEBUG("empty receiver");
                            }
                            else if (receiver->parent_process() == *process_information::get()) {
                                // this message was send from a proxy
                                receiver->attach_functor([=](std::uint32_t reason) {
                                    addressed_message kmsg{receiver, receiver, make_any_tuple(atom("KILL_PROXY"), reason)};
                                    singleton_manager::get_network_manager()
                                    ->send_to_mailman(make_any_tuple(m_peer, kmsg));
                                });
                            }
                            else {
                                DEBUG("MONITOR received for a remote actor");
                            }
                        },
                        on(atom("LINK"), arg_match) >> [&](actor_ptr ptr) {
                            if (msg.sender()->is_proxy() == false) {
                                DEBUG("msg.sender() is not a proxy");
                                return;
                            }
                            auto whom = msg.sender().downcast<actor_proxy>();
                            if ((whom) && (ptr)) whom->local_link_to(ptr);
                        },
                        on(atom("UNLINK"), arg_match) >> [](actor_ptr ptr) {
                            if (ptr->is_proxy() == false) {
                                DEBUG("msg.sender() is not a proxy");
                                return;
                            }
                            auto whom = ptr.downcast<actor_proxy>();
                            if ((whom) && (ptr)) whom->local_unlink_from(ptr);
                        },
                        others() >> [&]() {
                            if (msg.receiver()) {
                                msg.receiver()->enqueue(msg.sender().get(),
                                                        std::move(msg.content()));
                            }
                            else {
                                DEBUG("empty receiver");
                            }
                        }
                    );
                    m_buf.reset(sizeof(std::uint32_t));
                    m_state = wait_for_msg_size;
                    break;
                }
                default: {
                    CPPA_CRITICAL("illegal state");
                }
            }
            // try to read more (next iteration)
        }
    }

};

// accepts new connections to a published actor
class po_doorman : public po_socket_handler {

    actor_id m_actor_id;
    // caches process_information::get()
    process_information_ptr m_pself;
    po_socket_handler_vector* new_handler;

 public:

    po_doorman(actor_id aid, native_socket_type fd, po_socket_handler_vector* v)
        : po_socket_handler(fd)
        , m_actor_id(aid), m_pself(process_information::get())
        , new_handler(v) {
    }

    bool is_doorman_of(actor_id aid) const {
        return m_actor_id == aid;
    }

    ~po_doorman() {
        closesocket(m_socket);
    }

    bool read_and_continue() {
        for (;;) {
            sockaddr addr;
            socklen_t addrlen;
            memset(&addr, 0, sizeof(addr));
            memset(&addrlen, 0, sizeof(addrlen));
            auto sfd = ::accept(m_socket, &addr, &addrlen);
            if (sfd < 0) {
                switch (errno) {
                    case EAGAIN:
#                   if EAGAIN != EWOULDBLOCK
                    case EWOULDBLOCK:
#                   endif
                        // just try again
                        return true;
                    default:
                        perror("accept()");
                        DEBUG("accept failed (actor unpublished?)");
                        return false;
                }
            }
            int flags = 1;
            setsockopt(sfd, IPPROTO_TCP, TCP_NODELAY, &flags, sizeof(int));
            flags = fcntl(sfd, F_GETFL, 0);
            if (flags == -1) {
                throw network_error("unable to get socket flags");
            }
            if (fcntl(sfd, F_SETFL, flags | O_NONBLOCK) < 0) {
                throw network_error("unable to set socket to nonblock");
            }
            auto id = m_actor_id;
            std::uint32_t process_id = m_pself->process_id();
            ::send(sfd, &id, sizeof(std::uint32_t), 0);
            ::send(sfd, &process_id, sizeof(std::uint32_t), 0);
            ::send(sfd, m_pself->node_id().data(), m_pself->node_id().size(), 0);
            new_handler->emplace_back(new po_peer(sfd));
            DEBUG("socket accepted; published actor: " << id);
        }
    }

};

inline constexpr std::uint64_t valof(atom_value val) {
    return static_cast<std::uint64_t>(val);
}

void post_office_loop(int input_fd) {
    int maxfd = 0;
    fd_set readset;
    bool done = false;
    po_socket_handler_vector handler;
    po_socket_handler_vector new_handler;
    do {
        FD_ZERO(&readset);
        FD_SET(input_fd, &readset);
        maxfd = input_fd;
        for (auto& hptr : handler) {
            auto fd = hptr->get_socket();
            maxfd = std::max(maxfd, fd);
            FD_SET(fd, &readset);
        }
        if (select(maxfd + 1, &readset, nullptr, nullptr, nullptr) < 0) {
            // must not happen
            DEBUG("select failed!");
            perror("select()");
            exit(3);
        }
        { // iterate over all handler and remove if needed
            auto i = handler.begin();
            while (i != handler.end()) {
                if (   FD_ISSET((*i)->get_socket(), &readset)
                    && (*i)->read_and_continue() == false) {
                    DEBUG("handler erased");
                    i = handler.erase(i);
                }
                else {
                    ++i;
                }
            }
        }
        if (FD_ISSET(input_fd, &readset)) {
            DEBUG("post_office: read from pipe");
            po_message msg;
            if (read(input_fd, &msg, sizeof(po_message)) != sizeof(po_message)) {
                CPPA_CRITICAL("cannot read from pipe");
            }
            switch (valof(msg.flag)) {
                case valof(atom("ADD_PEER")): {
                    receive (
                        on_arg_match >> [&](native_socket_type fd,
                                            process_information_ptr piptr) {
                            DEBUG("post_office: add_peer");
                            handler.emplace_back(new po_peer(fd, piptr));
                        }
                    );
                    break;
                }
                case valof(atom("RM_PEER")): {
                    DEBUG("post_office: rm_peer");
                    auto i = std::find_if(handler.begin(), handler.end(),
                                          [&](const po_socket_handler_ptr& hp) {
                        return hp->get_socket() == msg.fd;
                    });
                    if (i != handler.end()) handler.erase(i);
                    break;
                }
                case valof(atom("PUBLISH")): {
                    receive (
                        on_arg_match >> [&](native_socket_type sockfd,
                                            actor_ptr whom) {
                            DEBUG("post_office: publish_actor");
                            CPPA_REQUIRE(sockfd > 0);
                            CPPA_REQUIRE(whom.get() != nullptr);
                            handler.emplace_back(new po_doorman(whom->id(), sockfd, &new_handler));
                        }
                    );
                    break;
                }
                case valof(atom("UNPUBLISH")): {
                    DEBUG("post_office: unpublish_actor");
                    auto i = std::find_if(handler.begin(), handler.end(),
                                          [&](const po_socket_handler_ptr& hp) {
                        return hp->is_doorman_of(msg.aid);
                    });
                    if (i != handler.end()) handler.erase(i);
                    break;
                }
                case valof(atom("DONE")): {
                    done = true;
                    break;
                }
                default: {
                    CPPA_CRITICAL("illegal pipe message");
                }
            }
        }
        if (new_handler.empty() == false) {
            std::move(new_handler.begin(), new_handler.end(),
                      std::back_inserter(handler));
            new_handler.clear();
        }
    }
    while (done == false);
}

/******************************************************************************
 *                remaining implementation of post_office.hpp                 *
 *                 (forward each function call to our queue)                  *
 ******************************************************************************/

void post_office_add_peer(native_socket_type a0,
                          const process_information_ptr& a1) {
    po_message msg{atom("ADD_PEER"), -1, 0};
    send2po(msg, a0, a1);
}

void post_office_publish(native_socket_type server_socket,
                         const actor_ptr& published_actor) {
    po_message msg{atom("PUBLISH"), -1, 0};
    send2po(msg, server_socket, published_actor);
}

void post_office_unpublish(actor_id whom) {
    po_message msg{atom("UNPUBLISH"), -1, whom};
    send2po(msg);
}

void post_office_close_socket(native_socket_type sfd) {
    po_message msg{atom("RM_PEER"), sfd, 0};
    send2po(msg);
}

} } // namespace cppa::detail
