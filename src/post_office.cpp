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

// used cppa classes
#include "cppa/atom.hpp"
#include "cppa/to_string.hpp"
#include "cppa/deserializer.hpp"
#include "cppa/binary_deserializer.hpp"

// used cppa utility
#include "cppa/util/single_reader_queue.hpp"

// used cppa details
#include "cppa/detail/thread.hpp"
#include "cppa/detail/buffer.hpp"
#include "cppa/detail/mailman.hpp"
#include "cppa/detail/post_office.hpp"
#include "cppa/detail/native_socket.hpp"
#include "cppa/detail/actor_registry.hpp"
#include "cppa/detail/network_manager.hpp"
#include "cppa/detail/post_office_msg.hpp"
#include "cppa/detail/singleton_manager.hpp"
#include "cppa/detail/actor_proxy_cache.hpp"
#include "cppa/detail/addressed_message.hpp"

/*
#define DEBUG(arg)                                                             \
    std::cout << "[process id: "                                               \
              << cppa::process_information::get()->process_id()                \
              << "] " << arg << std::endl
//*/

#define DEBUG(unused) ((void) 0)

using std::cerr;
using std::endl;

namespace {

// allocate in 1KB chunks (minimize reallocations)
constexpr size_t s_chunk_size = 1024;

// allow up to 1MB per buffer
constexpr size_t s_max_buffer_size = (1024 * 1024);

static_assert((s_max_buffer_size % s_chunk_size) == 0,
              "max_buffer_size is not a multiple of chunk_size");

static_assert(sizeof(cppa::detail::native_socket_type) == sizeof(std::uint32_t),
              "sizeof(native_socket_t) != sizeof(std::uint32_t)");

constexpr int s_rdflag = MSG_DONTWAIT;

} // namespace <anonmyous>

namespace cppa { namespace detail {

util::single_reader_queue<mailman_job>& mailman_queue()
{
    return singleton_manager::get_network_manager()->mailman_queue();
}

class po_doorman;

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
    // TCP socket identifying our parent (that accepted m_socket)
    native_socket_type m_parent_socket;
    // caches process_information::get()
    process_information_ptr m_pself;
    // the process information or our remote peer
    process_information_ptr m_peer;
    std::unique_ptr<attachable> m_observer;
    buffer<s_chunk_size, s_max_buffer_size> m_rdbuf;
    std::list<actor_proxy_ptr> m_children;

    const uniform_type_info* m_meta_msg;

 public:

    explicit po_peer(post_office_msg::add_peer& from)
        : m_state(wait_for_msg_size)
        , m_socket(from.sockfd)
        , m_parent_socket(-1)
        , m_pself(process_information::get())
        , m_peer(std::move(from.peer))
        , m_observer(std::move(from.attachable_ptr))
        , m_meta_msg(uniform_typeid<detail::addressed_message>())
    {
    }

    explicit po_peer(native_socket_type sockfd, native_socket_type parent_socket)
        : m_state(wait_for_process_info)
        , m_socket(sockfd)
        , m_parent_socket(parent_socket)
        , m_pself(process_information::get())
        , m_meta_msg(uniform_typeid<detail::addressed_message>())
    {
        m_rdbuf.reset(  sizeof(std::uint32_t)
                      + process_information::node_id_size);
    }

    po_peer(po_peer&& other)
        : m_state(other.m_state)
        , m_socket(other.m_socket)
        , m_parent_socket(other.m_parent_socket)
        , m_pself(process_information::get())
        , m_peer(std::move(other.m_peer))
        , m_observer(std::move(other.m_observer))
        , m_rdbuf(std::move(other.m_rdbuf))
        , m_children(std::move(other.m_children))
        , m_meta_msg(other.m_meta_msg)
    {
        other.m_socket = -1;
        other.m_parent_socket = -1;
        // other.m_children.clear();
    }

    native_socket_type get_socket() const
    {
        return m_socket;
    }

    // returns true if @p pod is the parent of this
    inline bool parent_exited(const po_doorman& pod);

    void add_child(const actor_proxy_ptr& pptr)
    {
        if (pptr) m_children.push_back(pptr);
        else
        {
            DEBUG("po_peer::add_child(nullptr) called");
        }
    }

    inline size_t children_count() const
    {
        return m_children.size();
    }

    inline bool has_parent() const
    {
        return m_parent_socket != -1;
    }

    // removes pptr from the list of children and returns
    // a <bool, size_t> pair, whereas: first = true if pptr is a child of this
    //                                 second = number of remaining children
    std::pair<bool, size_t> remove_child(const actor_proxy_ptr& pptr)
    {
        auto end = m_children.end();
        auto i = std::find(m_children.begin(), end, pptr);
        if (i != end)
        {
            m_children.erase(i);
            return { true, m_children.size() };
        }
        return { false, m_children.size() };
    }


    ~po_peer()
    {
        if (!m_children.empty())
        {
            auto msg = make_tuple(atom(":KillProxy"),
                                  exit_reason::remote_link_unreachable);
            for (actor_proxy_ptr& pptr : m_children)
            {
                pptr->enqueue(nullptr, msg);
            }
        }
        if (m_socket != -1) closesocket(m_socket);
    }

    // @returns false if an error occured; otherwise true
    bool read_and_continue()
    {
        static constexpr size_t wfp_size =   sizeof(std::uint32_t)
                                           + process_information::node_id_size;
        switch (m_state)
        {
            case wait_for_process_info:
            {
                if (m_rdbuf.final_size() != wfp_size)
                {
                    m_rdbuf.reset(wfp_size);
                }
                if (m_rdbuf.append_from(m_socket, s_rdflag) == false)
                {
                    return false;
                }
                if (m_rdbuf.ready() == false)
                {
                    break;
                }
                else
                {
                    std::uint32_t process_id;
                    memcpy(&process_id, m_rdbuf.data(), sizeof(std::uint32_t));
                    process_information::node_id_type node_id;
                    memcpy(node_id.data(),
                           m_rdbuf.data() + sizeof(std::uint32_t),
                           process_information::node_id_size);
                    m_peer.reset(new process_information(process_id, node_id));
                    // inform mailman about new peer
                    mailman_queue().push_back(new mailman_job(m_socket,
                                                              m_peer));
                    m_rdbuf.reset();
                    m_state = wait_for_msg_size;
                    DEBUG("pinfo read: "
                          << m_peer->process_id()
                          << "@"
                          << to_string(m_peer->node_id()));
                    // fall through and try to read more from socket
                }
            }
            case wait_for_msg_size:
            {
                if (m_rdbuf.final_size() != sizeof(std::uint32_t))
                {
                    m_rdbuf.reset(sizeof(std::uint32_t));
                }
                if (!m_rdbuf.append_from(m_socket, s_rdflag)) return false;
                if (m_rdbuf.ready() == false)
                {
                    break;
                }
                else
                {
                    // read and set message size
                    std::uint32_t msg_size;
                    memcpy(&msg_size, m_rdbuf.data(), sizeof(std::uint32_t));
                    m_rdbuf.reset(msg_size);
                    m_state = read_message;
                    // fall through and try to read more from socket
                }
            }
            case read_message:
            {
                if (!m_rdbuf.append_from(m_socket, s_rdflag))
                {
                    // could not read from socket
                    return false;
                }
                if (m_rdbuf.ready() == false)
                {
                    // wait for new data
                    break;
                }
                addressed_message msg;
                binary_deserializer bd(m_rdbuf.data(), m_rdbuf.size());
                try
                {
                    m_meta_msg->deserialize(&msg, &bd);
                }
                catch (std::exception& e)
                {
                    // unable to deserialize message (format error)
                    DEBUG(to_uniform_name(typeid(e)) << ": " << e.what());
                    return false;
                }
                auto& content = msg.content();
                DEBUG("<-- " << to_string(content));
                if (   content.size() == 1
                    && *(content.type_at(0)) == typeid(atom_value)
                    && content.get_as<atom_value>(0) == atom(":Monitor"))
                {
                    auto receiver = msg.receiver().downcast<actor>();
                    //actor_ptr receiver = dynamic_cast<actor*>(receiver_ch.get());
                    if (!receiver)
                    {
                        DEBUG("empty receiver");
                    }
                    else if (receiver->parent_process() == *process_information::get())
                    {
                        //cout << pinfo << " ':Monitor'; actor id = "
                        //     << sender->id() << endl;
                        // local actor?
                        // this message was send from a proxy
                        receiver->attach_functor([=](std::uint32_t reason)
                        {
                            any_tuple kmsg = make_tuple(atom(":KillProxy"),
                                                        reason);
                            auto mjob = new detail::mailman_job(m_peer,
                                                                receiver,
                                                                receiver,
                                                                kmsg);
                            detail::mailman_queue().push_back(mjob);
                        });
                    }
                    else
                    {
                        DEBUG(":Monitor received for an remote actor");
                    }
                }
                else
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
                m_rdbuf.reset();
                m_state = wait_for_msg_size;
                break;
            }
            default:
            {
                throw std::logic_error("illegal state");
            }
        }
        return true;
    }

};

// accepts new connections to a published actor
class po_doorman
{

    // server socket
    native_socket_type m_socket;
    actor_ptr published_actor;
    std::list<po_peer>* m_peers;
    // caches process_information::get()
    process_information_ptr m_pself;

 public:

    explicit po_doorman(post_office_msg::add_server_socket& assm,
                        std::list<po_peer>* peers)
        : m_socket(assm.server_sockfd)
        , published_actor(assm.published_actor)
        , m_peers(peers)
        , m_pself(process_information::get())
    {
    }

    ~po_doorman()
    {
        if (m_socket != -1) closesocket(m_socket);
    }

    po_doorman(po_doorman&& other)
        : m_socket(other.m_socket)
        , published_actor(std::move(other.published_actor))
        , m_peers(other.m_peers)
        , m_pself(process_information::get())
    {
        other.m_socket = -1;
    }

    inline native_socket_type get_socket() const
    {
        return m_socket;
    }

    // @returns false if an error occured; otherwise true
    bool read_and_continue()
    {
        sockaddr addr;
        socklen_t addrlen;
        memset(&addr, 0, sizeof(addr));
        memset(&addrlen, 0, sizeof(addrlen));
        auto sfd = ::accept(m_socket, &addr, &addrlen);
        if (sfd < 0)
        {
            switch (errno)
            {
                case EAGAIN:
#               if EAGAIN != EWOULDBLOCK
                case EWOULDBLOCK:
#               endif
                {
                    // just try again
                    return true;
                }
                default: return false;
            }
        }
        auto id = published_actor->id();
        std::uint32_t process_id = m_pself->process_id();
        ::send(sfd, &id, sizeof(std::uint32_t), 0);
        ::send(sfd, &process_id, sizeof(std::uint32_t), 0);
        ::send(sfd, m_pself->node_id().data(), m_pself->node_id().size(), 0);
        m_peers->push_back(po_peer(sfd, m_socket));
        DEBUG("socket accepted; published actor: " << id);
        return true;
    }

};

inline bool po_peer::parent_exited(const po_doorman& pod)
{
    if (m_parent_socket == pod.get_socket())
    {
        m_parent_socket = -1;
        return true;
    }
    return false;
}

// starts and stops mailman_loop
struct mailman_worker
{
    thread m_thread;
    mailman_worker() : m_thread(mailman_loop)
    {
    }
    ~mailman_worker()
    {
        mailman_queue().push_back(mailman_job::kill_job());
        m_thread.join();
    }
};

void post_office_loop(int pipe_read_handle, int pipe_write_handle)
{
    mailman_worker mworker;
    // map of all published actors (actor_id => list<doorman>)
    std::map<std::uint32_t, std::list<po_doorman> > doormen;
    // list of all connected peers
    std::list<po_peer> peers;
    // readset for select()
    fd_set readset;
    // maximum number of all socket descriptors for select()
    int maxfd = 0;
    // initialize variables for select()
    FD_ZERO(&readset);
    maxfd = pipe_read_handle;
    FD_SET(pipe_read_handle, &readset);
    // keeps track about what peer we are iterating at the moment
    po_peer* selected_peer = nullptr;
    // our event queue
    auto& msg_queue = singleton_manager::get_network_manager()->post_office_queue();
    auto pself = process_information::get();
    // needed for lookups in our proxy cache
    actor_proxy_cache::key_tuple proxy_cache_key ( 0, // set on lookup
                                                   pself->process_id(),
                                                   pself->node_id()   );
    // initialize proxy cache
    get_actor_proxy_cache().set_new_proxy_callback([&](actor_proxy_ptr& pptr)
    {
        DEBUG("new_proxy_callback, actor id = " << pptr->id());
        // it's ok to access objects on the stack, since this callback
        // is guaranteed to be executed in the same thread
        if (selected_peer == nullptr)
        {
            if (!pptr) DEBUG("pptr == nullptr");
            throw std::logic_error("selected_peer == nullptr");
        }
        pptr->enqueue(nullptr, make_tuple(atom(":Monitor")));
        selected_peer->add_child(pptr);
        auto aid = pptr->id();
        auto pptr_copy = pptr;
        pptr->attach_functor([&msg_queue,aid,pipe_write_handle,pptr_copy] (std::uint32_t)
        {
            // this callback is not guaranteed to be executed in the same thread
            msg_queue.push_back(new post_office_msg(pptr_copy));
            pipe_msg msg = { rd_queue_event, 0 };
            if (write(pipe_write_handle, msg, pipe_msg_size) != (int) pipe_msg_size)
            {
                cerr << "FATAL: cannot write to pipe" << endl;
                abort();
            }
        });
    });
    for (;;)
    {
        if (select(maxfd + 1, &readset, nullptr, nullptr, nullptr) < 0)
        {
            // must not happen
            perror("select()");
            exit(3);
        }
        // iterate over all peers and remove peers on errors
        peers.remove_if([&](po_peer& peer) -> bool
        {
            if (FD_ISSET(peer.get_socket(), &readset))
            {
                selected_peer = &peer;
                return peer.read_and_continue() == false;
            }
            return false;
        });
        selected_peer = nullptr;
        // iterate over all doormen (accept new connections)
        // and remove doormen on errors
        for (auto& kvp : doormen)
        {
            // iterate over all doormen and remove doormen on error
            kvp.second.remove_if([&](po_doorman& doorman) -> bool
            {
                return    FD_ISSET(doorman.get_socket(), &readset)
                       && doorman.read_and_continue() == false;
            });
        }
        // read events from pipe
        if (FD_ISSET(pipe_read_handle, &readset))
        {
            pipe_msg pmsg;
            //memcpy(pmsg, pipe_msg_buf.data(), pipe_msg_buf.size());
            //pipe_msg_buf.clear();
            if (::read(pipe_read_handle, &pmsg, pipe_msg_size) != (int) pipe_msg_size)
            {
                cerr << "FATAL: cannot read from pipe" << endl;
                abort();
            }
            switch (pmsg[0])
            {
                case rd_queue_event:
                {
                    DEBUG("rd_queue_event");
                    post_office_msg* pom = msg_queue.pop();
                    if (pom->is_add_peer_msg())
                    {
                        DEBUG("add_peer_msg");
                        auto& apm = pom->as_add_peer_msg();
                        actor_proxy_ptr pptr = apm.first_peer_actor;
                        peers.push_back(po_peer(apm));
                        selected_peer = &(peers.back());
                        if (pptr)
                        {
                            DEBUG("proxy added via post_office_msg");
                            get_actor_proxy_cache().add(pptr);
                        }
                        selected_peer = nullptr;
                    }
                    else if (pom->is_add_server_socket_msg())
                    {
                        DEBUG("add_server_socket_msg");
                        auto& assm = pom->as_add_server_socket_msg();
                        auto& pactor = assm.published_actor;
                        if (pactor)
                        {
                            auto aid = pactor->id();
                            auto callback = [aid](std::uint32_t)
                            {
                                DEBUG("call post_office_unpublish() ...");
                                post_office_unpublish(aid);
                            };
                            if (pactor->attach_functor(std::move(callback)))
                            {
                                auto& dm = doormen[aid];
                                dm.push_back(po_doorman(assm, &peers));
                                DEBUG("new doorman");
                            }
                        }
                        else
                        {
                            DEBUG("nullptr published");
                        }
                    }
                    else if (pom->is_proxy_exited_msg())
                    {
                        DEBUG("proxy_exited_msg");
                        auto pptr = std::move(pom->as_proxy_exited_msg().proxy_ptr);
                        if (pptr)
                        {
                            // get parent of pptr
                            auto i = peers.begin();
                            auto end = peers.end();
                            DEBUG("search parent of exited proxy");
                            while (i != end)
                            {
                                auto result = i->remove_child(pptr);
                                if (result.first) // true if pptr is a child
                                {
                                    DEBUG("found parent of proxy");
                                    if (result.second == 0) // no more children?
                                    {
                                        // disconnect peer if we don't know any
                                        // actor of it and if the published
                                        // actor already exited
                                        // (this is the case if the peer doesn't
                                        //  have a parent)
                                        if (i->has_parent() == false)
                                        {
                                            DEBUG("removed peer");
                                            peers.erase(i);
                                        }
                                    }
                                    i = end; // done
                                }
                                else
                                {
                                    DEBUG("... next iteration");
                                    ++i; // next iteration
                                }
                            }
                        }
                        else DEBUG("pptr == nullptr");
                    }
                    delete pom;
                    break;
                }
                case unpublish_actor_event:
                {
                    DEBUG("unpublish_actor_event");
                    auto kvp = doormen.find(pmsg[1]);
                    if (kvp != doormen.end())
                    {
                        DEBUG("erase doorman from map");
                        for (po_doorman& dm : kvp->second)
                        {
                            // remove peers with no children and no parent
                            // (that are peers that connected to an already
                            //  exited actor and where we don't know any
                            //  actor from)
                            peers.remove_if([&](po_peer& ppeer)
                            {
                                return    ppeer.parent_exited(dm)
                                       && ppeer.children_count() == 0;
                            });
                        }
                        doormen.erase(kvp);
                    }
                    break;
                }
                case close_socket_event:
                {
                    DEBUG("close_socket_event");
                    auto sockfd = static_cast<native_socket_type>(pmsg[1]);
                    auto end = peers.end();
                    auto i = std::find_if(peers.begin(), end,
                                          [sockfd](po_peer& peer) -> bool
                    {
                        return peer.get_socket() == sockfd;
                    });
                    if (i != end) peers.erase(i);
                    break;
                }
                case shutdown_event:
                {
                    // goodbye
                    return;
                }
                default:
                {
                    std::ostringstream oss;
                    oss << "unexpected event type: " << pmsg[0];
                    throw std::logic_error(oss.str());
                }
            }
        }
        // recalculate readset
        FD_ZERO(&readset);
        FD_SET(pipe_read_handle, &readset);
        maxfd = pipe_read_handle;
        for (po_peer& pd : peers)
        {
            auto fd = pd.get_socket();
            if (fd > maxfd) maxfd = fd;
            FD_SET(fd, &readset);
        }
        // iterate over key-value (actor id / doormen) pairs
        for (auto& kvp : doormen)
        {
            // iterate over values (doormen)
            for (auto& dm : kvp.second)
            {
                auto fd = dm.get_socket();
                if (fd > maxfd) maxfd = fd;
                FD_SET(fd, &readset);
            }
        }
    }
}

/******************************************************************************
 *                remaining implementation of post_office.hpp                 *
 *                 (forward each function call to our queue)                  *
 ******************************************************************************/

void post_office_add_peer(native_socket_type a0,
                          const process_information_ptr& a1,
                          const actor_proxy_ptr& a2,
                          std::unique_ptr<attachable>&& a3)
{
    auto nm = singleton_manager::get_network_manager();
    nm->post_office_queue().push_back(new post_office_msg(a0, a1, a2,
                                                          std::move(a3)));
    pipe_msg msg = { rd_queue_event, 0 };
    nm->write_to_pipe(msg);
}

void post_office_publish(native_socket_type server_socket,
                         const actor_ptr& published_actor)
{
    DEBUG("post_office_publish(" << published_actor->id() << ")");
    auto nm = singleton_manager::get_network_manager();
    nm->post_office_queue().push_back(new post_office_msg(server_socket,
                                                          published_actor));
    pipe_msg msg = { rd_queue_event, 0 };
    nm->write_to_pipe(msg);
}

void post_office_unpublish(actor_id whom)
{
    DEBUG("post_office_unpublish(" << whom << ")");
    auto nm = singleton_manager::get_network_manager();
    pipe_msg msg = { unpublish_actor_event, whom };
    nm->write_to_pipe(msg);
}

void post_office_close_socket(native_socket_type sfd)
{
    auto nm = singleton_manager::get_network_manager();
    pipe_msg msg = { close_socket_event, static_cast<std::uint32_t>(sfd) };
    nm->write_to_pipe(msg);
}

} } // namespace cppa::detail
