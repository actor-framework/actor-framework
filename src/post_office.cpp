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

#include "cppa/util/acceptor.hpp"
#include "cppa/util/io_stream.hpp"
#include "cppa/util/input_stream.hpp"
#include "cppa/util/output_stream.hpp"

#include "cppa/detail/buffer.hpp"
#include "cppa/detail/mailman.hpp"
#include "cppa/detail/types_array.hpp"
#include "cppa/detail/post_office.hpp"
#include "cppa/detail/actor_registry.hpp"
#include "cppa/detail/network_manager.hpp"
#include "cppa/detail/singleton_manager.hpp"
#include "cppa/detail/actor_proxy_cache.hpp"
#include "cppa/detail/addressed_message.hpp"

#define DEBUG(arg)                                                             \
    {                                                                          \
        std::ostringstream oss;                                                \
        oss << "[process id: "                                                 \
            << cppa::process_information::get()->process_id()                  \
            << "] " << arg << std::endl;                                       \
        std::cout << oss.str();                                                \
    } (void) 0

#undef DEBUG
#define DEBUG(unused) ((void) 0)

using std::cout;
using std::cerr;
using std::endl;

namespace cppa { namespace detail {

namespace {

types_array<atom_value, actor_ptr> t_atom_actor_ptr_types;

// allocate in 1KB chunks (minimize reallocations)
constexpr size_t s_chunk_size = 1024;

// allow up to 1MB per buffer
constexpr size_t s_max_buffer_size = (1024 * 1024);

static_assert((s_max_buffer_size % s_chunk_size) == 0,
              "max_buffer_size is not a multiple of chunk_size");

static_assert(sizeof(native_socket_type) == sizeof(std::uint32_t),
              "sizeof(native_socket_t) != sizeof(std::uint32_t)");

template<typename... Args>
inline void send2po(Args&&... args) {
    auto nm = singleton_manager::get_network_manager();
    nm->send_to_post_office(std::unique_ptr<po_message>(new po_message(std::forward<Args>(args)...)));
}

template<typename T, typename... Args>
void call_ctor(T& var, Args&&... args) {
    new (&var) T (std::forward<Args>(args)...);
}

template<typename T>
void call_dtor(T& var) {
    var.~T();
}

} // namespace <anonmyous>

po_message::po_message() : next(0), type(po_message_type::shutdown) { }

po_message::po_message(util::io_stream_ptr_pair a0, process_information_ptr a1)
: next(0), type(po_message_type::add_peer) {
    call_ctor(new_peer, std::move(a0), std::move(a1));
}

po_message::po_message(util::io_stream_ptr_pair a0)
: next(0), type(po_message_type::rm_peer) {
    call_ctor(peer_streams, std::move(a0));
}

po_message::po_message(std::unique_ptr<util::acceptor> a0, actor_ptr a1)
: next(0), type(po_message_type::publish) {
    call_ctor(new_published_actor, std::move(a0), std::move(a1));
}

po_message::po_message(actor_ptr a0)
: next(0), type(po_message_type::unpublish) {
    call_ctor(published_actor, std::move(a0));
}

po_message::~po_message() {
    switch (type) {
        case po_message_type::add_peer: {
            call_dtor(new_peer);
            break;
        }
        case po_message_type::rm_peer: {
            call_dtor(peer_streams);
            break;
        }
        case po_message_type::publish: {
            call_dtor(new_published_actor);
            break;
        }
        case po_message_type::unpublish: {
            call_dtor(published_actor);
            break;
        }
        default: break;
    }
}

class post_office;

class post_office_worker {

 public:

    post_office_worker(post_office* parent) : m_parent(parent) { }

    virtual ~post_office_worker() { }

    // returns false if either done or an error occured
    virtual bool read_and_continue() = 0;

    virtual native_socket_type get_socket() const = 0;

    virtual bool is_doorman_of(actor_id) const { return false; }

 protected:

    post_office* parent() { return m_parent; }

 private:

    post_office* m_parent;

};

typedef std::unique_ptr<post_office_worker> po_worker_ptr;
typedef std::vector<po_worker_ptr> po_worker_vector;

class post_office {

    friend void post_office_loop(int, po_message_queue&);

 public:

    post_office() : m_done(false), m_pself(process_information::get()) {
        DEBUG("started post office at "
              << m_pself->process_id() << "@" << to_string(m_pself->node_id()));
    }

    template<class Worker, typename... Args>
    inline void add_worker(Args&&... args) {
        m_new_workers.emplace_back(new Worker(this, std::forward<Args>(args)...));
    }

    inline void close_socket(native_socket_type fd) {
        m_closed_sockets.push_back(fd);
    }

    inline void quit() {
        m_done = true;
    }

    post_office_worker* doorman_of(actor_id whom) {
        auto last = m_workers.end();
        auto i = std::find_if(m_workers.begin(), last,
                              [whom](const po_worker_ptr& hp) {
            return hp->is_doorman_of(whom);
        });
        return (i != last) ? i->get() : nullptr;
    }

    const process_information_ptr& pself() {
        return m_pself;
    }

 private:

    void operator()(int input_fd, po_message_queue& q);

    bool m_done;
    process_information_ptr m_pself;
    po_worker_vector m_workers;
    po_worker_vector m_new_workers;
    std::vector<native_socket_type> m_closed_sockets;

};

// represents a TCP connection to another peer
class po_peer : public post_office_worker {

    enum state {
        // connection just established; waiting for process information
        wait_for_process_info,
        // wait for the size of the next message
        wait_for_msg_size,
        // currently reading a message
        read_message
    };

    util::input_stream_ptr m_istream;
    util::output_stream_ptr m_ostream;

    state m_state;
    // the process information of our remote peer
    process_information_ptr m_peer;
    // caches uniform_typeid<addressed_message>()
    const uniform_type_info* m_meta_msg;
    // manages socket input
    buffer<512, (16 * 1024 * 1024)> m_buf;

 public:

    po_peer(post_office* parent,
            util::io_stream_ptr_pair spair,
            process_information_ptr peer = nullptr)
    : post_office_worker(parent)
    , m_istream(std::move(spair.first))
    , m_ostream(std::move(spair.second))
    , m_state((peer) ? wait_for_msg_size : wait_for_process_info)
    , m_peer(peer)
    , m_meta_msg(uniform_typeid<addressed_message>()) {
        m_buf.reset(m_state == wait_for_process_info
                    ? sizeof(std::uint32_t) + process_information::node_id_size
                    : sizeof(std::uint32_t));
    }

    ~po_peer() {
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

    native_socket_type get_socket() const {
        return m_istream->read_file_handle();
    }

    // @returns false if an error occured; otherwise true
    bool read_and_continue() {
        for (;;) {
            try {
                m_buf.append_from(m_istream.get());
            }
            catch (std::exception& e) {
                DEBUG(e.what());
                return false;
            }
            if (!m_buf.full()) {
                return true; // try again later
            }
            switch (m_state) {
                case wait_for_process_info: {
                    DEBUG("po_peer: read_and_continue: wait_for_process_info");
                    std::uint32_t process_id;
                    process_information::node_id_type node_id;
                    memcpy(&process_id, m_buf.data(), sizeof(std::uint32_t));
                    memcpy(node_id.data(), m_buf.data() + sizeof(std::uint32_t),
                           process_information::node_id_size);
                    m_peer.reset(new process_information(process_id, node_id));
                    util::io_stream_ptr_pair iop(m_istream, m_ostream);
                    DEBUG("po_peer: send new peer to mailman");
                    // inform mailman about new peer
                    mailman_add_peer(iop, m_peer);
                    // initialization done
                    m_state = wait_for_msg_size;
                    m_buf.reset(sizeof(std::uint32_t));
                    DEBUG("pinfo read: "
                          << m_peer->process_id()
                          << "@"
                          << to_string(m_peer->node_id()));
                    break;
                }
                case wait_for_msg_size: {
                    DEBUG("po_peer: read_and_continue: wait_for_msg_size");
                    std::uint32_t msg_size;
                    memcpy(&msg_size, m_buf.data(), sizeof(std::uint32_t));
                    DEBUG("msg_size: " << msg_size);
                    try {
                        m_buf.reset(msg_size);
                    }
                    catch (std::exception& e) {
                        DEBUG(e.what());
                        return false;
                    }
                    m_state = read_message;
                    break;
                }
                case read_message: {
                    DEBUG("po_peer: read_and_continue: read_message");
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
                                auto mpeer = m_peer;
                                // this message was send from a proxy
                                receiver->attach_functor([mpeer, receiver](std::uint32_t reason) {
                                    addressed_message kmsg{receiver, receiver, make_any_tuple(atom("KILL_PROXY"), reason)};
                                    singleton_manager::get_network_manager()
                                    ->send_to_mailman(mm_message::create(mpeer, kmsg));
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
                            auto receiver = msg.receiver().get();
                            if (receiver) {
                                if (msg.id().valid()) {
                                    auto ra = dynamic_cast<actor*>(receiver);
                                    if (ra) {
                                        ra->sync_enqueue(
                                            msg.sender().get(),
                                            msg.id(),
                                            std::move(msg.content()));
                                    }
                                    else{
                                        DEBUG("sync message to a non-actor");
                                    }
                                }
                                else {
                                    receiver->enqueue(
                                        msg.sender().get(),
                                        std::move(msg.content()));
                                }
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
class po_doorman : public post_office_worker {

 public:

    po_doorman(post_office* parent,
               actor_id aid,
               std::unique_ptr<util::acceptor> acceptor)
    : post_office_worker(parent)
    , m_actor_id(aid)
    , m_acceptor(std::move(acceptor)) {
    }

    bool is_doorman_of(actor_id aid) const {
        return m_actor_id == aid;
    }

    native_socket_type get_socket() const {
        return m_acceptor->acceptor_file_handle();
    }

    bool read_and_continue() {
        // accept as many connections as possible
        for (;;) {
            auto opt = m_acceptor->try_accept_connection();
            if (opt) {
                auto& pair = *opt;
                auto& pself = parent()->pself();
                std::uint32_t process_id = pself->process_id();
                pair.second->write(&m_actor_id, sizeof(actor_id));
                pair.second->write(&process_id, sizeof(std::uint32_t));
                pair.second->write(pself->node_id().data(),
                                   pself->node_id().size());
                parent()->add_worker<po_peer>(pair);
                DEBUG("connection accepted; published actor: " << m_actor_id);
            }
            else {
                return true;
            }
        }
    }

 private:

    actor_id m_actor_id;
    std::unique_ptr<util::acceptor> m_acceptor;

};

class po_overseer : public post_office_worker {

 public:

    po_overseer(post_office* parent,
                int pipe_fd,
                po_message_queue& q)
    : post_office_worker(parent)
    , m_pipe_fd(pipe_fd)
    , m_queue(q) { }

    native_socket_type get_socket() const {
        return m_pipe_fd;
    }

    bool read_and_continue() {
        std::uint32_t dummy;
        if (::read(m_pipe_fd, &dummy, sizeof(dummy)) != sizeof(dummy)) {
            CPPA_CRITICAL("cannot read from pipe");
        }
        std::atomic_thread_fence(std::memory_order_seq_cst);
        std::unique_ptr<po_message> msg(m_queue.pop());
        switch (msg->type) {
            case po_message_type::add_peer: {
                DEBUG("post_office: add_peer");
                auto& new_peer = msg->new_peer;
                parent()->add_worker<po_peer>(new_peer.first, new_peer.second);
                break;
            }
            case po_message_type::rm_peer: {
                DEBUG("post_office: rm_peer");
                auto istream = msg->peer_streams.first;
                if (istream) {
                    parent()->close_socket(istream->read_file_handle());
                }
                break;
            }
            case po_message_type::publish: {
                DEBUG("post_office: publish");
                auto& ptrs = msg->new_published_actor;
                parent()->add_worker<po_doorman>(ptrs.second->id(),
                                                 std::move(ptrs.first));
                break;
            }
            case po_message_type::unpublish: {
                DEBUG("post_office: unpublish");
                if (msg->published_actor) {
                    auto aid = msg->published_actor->id();
                    auto worker = parent()->doorman_of(aid);
                    if (worker) {
                        parent()->close_socket(worker->get_socket());
                    }
                }
                break;
            }
            case po_message_type::shutdown: {
                DEBUG("post_office: shutdown");
                parent()->quit();
                break;
            }
        }
        return true;
    }

 private:

    int m_pipe_fd;
    po_message_queue& m_queue;

};

void post_office::operator()(int input_fd, po_message_queue& q) {
    int maxfd = 0;
    fd_set readset;
    m_workers.emplace_back(new po_overseer(this, input_fd, q));
    do {
        FD_ZERO(&readset);
        maxfd = 0;
        CPPA_REQUIRE(m_workers.size() > 0);
        for (auto& worker : m_workers) {
            auto fd = worker->get_socket();
            maxfd = std::max(maxfd, fd);
            FD_SET(fd, &readset);
        }
        CPPA_REQUIRE(maxfd > 0);
        if (select(maxfd + 1, &readset, nullptr, nullptr, nullptr) < 0) {
            // must not happen
            DEBUG("select failed!");
            perror("select()");
            exit(3);
        }
        { // iterate over all workers and remove workers as needed
            auto i = m_workers.begin();
            while (i != m_workers.end()) {
                if (   FD_ISSET((*i)->get_socket(), &readset)
                    && (*i)->read_and_continue() == false) {
                    DEBUG("erase worker (read_and_continue() returned false)");
                    i = m_workers.erase(i);
                }
                else {
                    ++i;
                }
            }
        }
        // erase all handlers with closed sockets
        for (auto fd : m_closed_sockets) {
            auto i = std::find_if(m_workers.begin(), m_workers.end(),
                                  [fd](const po_worker_ptr& wptr) {
                                      return wptr->get_socket() == fd;
            });
            if (i != m_workers.end()) {
                m_workers.erase(i);
            }
        }
        // insert new handlers
        if (m_new_workers.empty() == false) {
            std::move(m_new_workers.begin(), m_new_workers.end(),
                      std::back_inserter(m_workers));
            m_new_workers.clear();
        }
    }
    while (m_done == false);
    DEBUG("post_office_loop: done");
}

void post_office_loop(int input_fd, po_message_queue& q) {
    post_office po;
    po(input_fd, q);
}

} } // namespace cppa::detail
