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
#include <vector>       // std::vector
#include <cstring>      // strerror
#include <cstdint>      // std::uint32_t, std::uint64_t
#include <iostream>     // std::cout, std::cerr, std::endl
#include <exception>    // std::logic_error
#include <algorithm>    // std::find_if
#include <stdexcept>    // std::underflow_error
#include <sstream>

#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>

#include "cppa/cppa.hpp"
#include "cppa/atom.hpp"
#include "cppa/match.hpp"
#include "cppa/config.hpp"
#include "cppa/to_string.hpp"
#include "cppa/deserializer.hpp"
#include "cppa/binary_deserializer.hpp"

#include "cppa/detail/thread.hpp"
#include "cppa/detail/buffer.hpp"
#include "cppa/detail/mailman.hpp"
#include "cppa/detail/types_array.hpp"
#include "cppa/detail/post_office.hpp"
#include "cppa/detail/native_socket.hpp"
#include "cppa/detail/actor_registry.hpp"
#include "cppa/detail/network_manager.hpp"
#include "cppa/detail/post_office_msg.hpp"
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

template<typename... Args>
inline void send2po(Args&&... args)
{
    singleton_manager::get_network_manager()
    ->send_to_post_office(make_any_tuple(std::forward<Args>(args)...));
}

template<class Fun>
struct scope_guard
{
    Fun m_fun;
    scope_guard(Fun&& fun) : m_fun(fun) { }
    ~scope_guard() { m_fun(); }
};

template<class Fun>
scope_guard<Fun> make_scope_guard(Fun fun) { return {std::move(fun)}; }

// represents a TCP connection to another peer
class po_peer
{

    enum state
    {
        // connection just established; waiting for process information
        wait_for_process_info,
        // wait for the size of the next message
        wait_for_msg_size,
        // currently reading a message
        read_message
    };

    state m_state;
    // TCP socket to remote peer
    native_socket_type m_socket;
    // caches process_information::get()
    process_information_ptr m_pself;
    // the process information or our remote peer
    process_information_ptr m_peer;

    thread m_thread;

 public:

    po_peer(native_socket_type fd, process_information_ptr peer)
        : m_state((peer) ? wait_for_msg_size : wait_for_process_info)
        , m_socket(fd)
        , m_pself(process_information::get())
        , m_peer(std::move(peer))
    {
    }

    ~po_peer()
    {
        closesocket(m_socket);
        m_thread.join();
    }

    inline native_socket_type get_socket() const { return m_socket; }

    void start()
    {
        m_thread = thread{std::bind(&po_peer::operator(), this)};
    }

    // @returns false if an error occured; otherwise true
    void operator()()
    {
        static constexpr size_t wfp_size =   sizeof(std::uint32_t)
                                           + process_information::node_id_size;
        auto nm = singleton_manager::get_network_manager();
        auto meta_msg = uniform_typeid<addressed_message>();
        buffer<s_chunk_size, s_max_buffer_size> rdbuf;
        auto guard = make_scope_guard([&]()
        {
            send2po(atom("RM_PEER"), m_socket);
        });
        for (;;)
        {
            switch (m_state)
            {
                case wait_for_process_info:
                {
                    if (rdbuf.final_size() != wfp_size)
                    {
                        rdbuf.reset(wfp_size);
                    }
                    if (rdbuf.append_from(m_socket) == false)
                    {
                        DEBUG("m_rdbuf.append_from() failed");
                        return;
                    }
                    CPPA_REQUIRE(rdbuf.ready());
                    std::uint32_t process_id;
                    memcpy(&process_id, rdbuf.data(), sizeof(std::uint32_t));
                    process_information::node_id_type node_id;
                    memcpy(node_id.data(),
                           rdbuf.data() + sizeof(std::uint32_t),
                           process_information::node_id_size);
                    m_peer.reset(new process_information(process_id, node_id));
                    // inform mailman about new peer
                    nm->send_to_mailman(make_any_tuple(m_socket, m_peer));
                    rdbuf.reset();
                    m_state = wait_for_msg_size;
                    DEBUG("pinfo read: "
                          << m_peer->process_id()
                          << "@"
                          << to_string(m_peer->node_id()));
                    // fall through and try to read more from socket
                }
                case wait_for_msg_size:
                {
                    if (rdbuf.final_size() != sizeof(std::uint32_t))
                    {
                        rdbuf.reset(sizeof(std::uint32_t));
                    }
                    if (!rdbuf.append_from(m_socket))
                    {
                        DEBUG("m_rdbuf.append_from() failed");
                        return;
                    }
                    CPPA_REQUIRE(rdbuf.ready());
                    // read and set message size
                    std::uint32_t msg_size;
                    memcpy(&msg_size, rdbuf.data(), sizeof(std::uint32_t));
                    rdbuf.reset(msg_size);
                    m_state = read_message;
                    // fall through and try to read more from socket
                }
                case read_message:
                {
                    if (!rdbuf.append_from(m_socket))
                    {
                        DEBUG("m_rdbuf.append_from() failed");
                        return;
                    }
                    CPPA_REQUIRE(rdbuf.ready());
                    addressed_message msg;
                    binary_deserializer bd(rdbuf.data(), rdbuf.size());
                    try
                    {
                        meta_msg->deserialize(&msg, &bd);
                    }
                    catch (std::exception& e)
                    {
                        // unable to deserialize message (format error)
                        DEBUG(to_uniform_name(typeid(e)) << ": " << e.what());
                        return;
                    }
                    auto& content = msg.content();
                    DEBUG("<-- " << to_string(msg));
                    match(content)
                    (
                        on(atom("MONITOR")) >> [&]()
                        {
                            auto receiver = msg.receiver().downcast<actor>();
                            CPPA_REQUIRE(receiver.get() != nullptr);
                            if (!receiver)
                            {
                                DEBUG("empty receiver");
                            }
                            else if (receiver->parent_process() == *process_information::get())
                            {
                                // this message was send from a proxy
                                receiver->attach_functor([=](std::uint32_t reason)
                                {
                                    addressed_message kmsg{receiver, receiver, make_any_tuple(atom("KILL_PROXY"), reason)};
                                    nm->send_to_mailman(make_any_tuple(m_peer, kmsg));
                                });
                            }
                            else
                            {
                                DEBUG("MONITOR received for a remote actor");
                            }
                        },
                        on(atom("LINK"), arg_match) >> [&](actor_ptr ptr)
                        {
                            if (msg.sender()->is_proxy() == false)
                            {
                                DEBUG("msg.sender() is not a proxy");
                                return;
                            }
                            auto whom = msg.sender().downcast<actor_proxy>();
                            if ((whom) && (ptr)) whom->local_link_to(ptr);
                        },
                        on(atom("UNLINK"), arg_match) >> [](actor_ptr ptr)
                        {
                            if (ptr->is_proxy() == false)
                            {
                                DEBUG("msg.sender() is not a proxy");
                                return;
                            }
                            auto whom = ptr.downcast<actor_proxy>();
                            if ((whom) && (ptr)) whom->local_unlink_from(ptr);
                        },
                        others() >> [&]()
                        {
                            if (msg.receiver())
                            {
                                msg.receiver()->enqueue(msg.sender().get(),
                                                        std::move(msg.content()));
                            }
                            else
                            {
                                DEBUG("empty receiver");
                            }
                        }
                    );
                    rdbuf.reset();
                    m_state = wait_for_msg_size;
                    break;
                }
                default:
                {
                    CPPA_CRITICAL("illegal state");
                }
            }
        }
    }

};

// accepts new connections to a published actor
class po_doorman
{

    // server socket
    native_socket_type m_socket;
    actor_ptr published_actor;
    // caches process_information::get()
    process_information_ptr m_pself;

    thread m_thread;

 public:

    po_doorman(native_socket_type fd, actor_ptr mactor)
        : m_socket(fd)
        , published_actor(std::move(mactor))
        , m_pself(process_information::get())
    {
    }

    ~po_doorman()
    {
        if (m_socket != -1) closesocket(m_socket);
        m_thread.join();
    }

    void start()
    {
        m_thread = thread{std::bind(&po_doorman::operator(), this)};
    }

    void operator()()
    {
        sockaddr addr;
        socklen_t addrlen;
        for (;;)
        {
            memset(&addr, 0, sizeof(addr));
            memset(&addrlen, 0, sizeof(addrlen));
            auto sfd = ::accept(m_socket, &addr, &addrlen);
            if (sfd < 0)
            {
                DEBUG("accept failed (actor unpublished?)");
                return;
            }
            auto id = published_actor->id();
            std::uint32_t process_id = m_pself->process_id();
            ::send(sfd, &id, sizeof(std::uint32_t), 0);
            ::send(sfd, &process_id, sizeof(std::uint32_t), 0);
            ::send(sfd, m_pself->node_id().data(), m_pself->node_id().size(), 0);
            send2po(atom("ADD_PEER"), sfd, process_information_ptr{});
            DEBUG("socket accepted; published actor: " << id);
        }
    }

};

void post_office_loop()
{
    bool done = false;
    // map of all published actors
    std::map<actor_id, std::list<po_doorman> > doormen;
    // list of all peers to which we established a connection via remote_actor()
    std::list<po_peer> peers;
    do_receive
    (
        on(atom("ADD_PEER"), arg_match) >> [&](native_socket_type fd,
                                               process_information_ptr piptr)
        {
            DEBUG("add_peer_msg");
            peers.emplace_back(fd, std::move(piptr));
            peers.back().start();
        },
        on(atom("RM_PEER"), arg_match) >> [&](native_socket_type fd)
        {
            DEBUG("rm_peer_msg");
            auto i = std::find_if(peers.begin(), peers.end(), [fd](po_peer& pp)
            {
                return pp.get_socket() == fd;
            });
            if (i != peers.end()) peers.erase(i);
        },
        on(atom("ADD_PROXY"), arg_match) >> [&](actor_proxy_ptr)
        {
            DEBUG("add_proxy_msg");
        },
        on(atom("RM_PROXY"), arg_match) >> [&](actor_proxy_ptr pptr)
        {
            DEBUG("rm_proxy_msg");
            CPPA_REQUIRE(pptr.get() != nullptr);
            get_actor_proxy_cache().erase(pptr);
        },
        on(atom("PUBLISH"), arg_match) >> [&](native_socket_type sockfd,
                                              actor_ptr whom)
        {
            DEBUG("unpublish_actor_event");
            CPPA_REQUIRE(whom.get() != nullptr);
            auto aid = whom->id();
            auto callback = [aid](std::uint32_t)
            {
                send2po(atom("UNPUBLISH"), aid);
            };
            if (whom->attach_functor(std::move(callback)))
            {
                auto& ls = doormen[aid];
                ls.emplace_back(sockfd, whom);
                ls.back().start();
                DEBUG("new doorman");
            }
            else
            {
                closesocket(sockfd);
            }
        },
        on(atom("UNPUBLISH"), arg_match) >> [&](actor_id whom)
        {
            DEBUG("unpublish_actor_event");
            doormen.erase(whom);
        },
        on(atom("DONE")) >> [&]()
        {
            done = true;
        },
        others() >> []()
        {
            std::string str = "unexpected message in post_office: ";
            str += to_string(self->last_dequeued());
            CPPA_CRITICAL(str.c_str());
        }
    )
    .until(gref(done));
}

/******************************************************************************
 *                remaining implementation of post_office.hpp                 *
 *                 (forward each function call to our queue)                  *
 ******************************************************************************/

void post_office_add_peer(native_socket_type a0,
                          const process_information_ptr& a1)
{
    send2po(atom("ADD_PEER"), a0, a1);
}

void post_office_publish(native_socket_type server_socket,
                         actor_ptr const& published_actor)
{
    send2po(atom("PUBLISH"), server_socket, published_actor);
}

void post_office_unpublish(actor_id whom)
{
    send2po(atom("UNPUBLISH"), whom);
}

void post_office_close_socket(native_socket_type sfd)
{
    send2po(atom("RM_PEER"), sfd);
}

} } // namespace cppa::detail
