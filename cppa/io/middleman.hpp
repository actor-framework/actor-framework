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
 * Copyright (C) 2011-2013                                                    *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation; either version 2.1 of the License,               *
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


#ifndef MIDDLEMAN_HPP
#define MIDDLEMAN_HPP

#include <map>
#include <vector>
#include <memory>
#include <functional>

#include "cppa/node_id.hpp"
#include "cppa/cppa_fwd.hpp"
#include "cppa/actor_namespace.hpp"

namespace cppa { namespace detail { class singleton_manager; } }

namespace cppa {

class actor_proxy;
typedef intrusive_ptr<actor_proxy> actor_proxy_ptr;
typedef weak_intrusive_ptr<actor_proxy> weak_actor_proxy_ptr;

} // namespace cppa

namespace cppa { namespace io {

class peer;
class continuable;
class input_stream;
class peer_acceptor;
class output_stream;
class middleman_event_handler;

typedef intrusive_ptr<input_stream> input_stream_ptr;
typedef intrusive_ptr<output_stream> output_stream_ptr;

/**
 * @brief Multiplexes asynchronous IO.
 * @note No member function except for @p run_later is safe to call from
 *       outside the event loop.
 */
class middleman {

    friend class detail::singleton_manager;

 public:

    virtual ~middleman();

    /**
     * @brief Runs @p fun in the event loop of the middleman.
     * @note This member function is thread-safe.
     */
    virtual void run_later(std::function<void()> fun) = 0;

    /**
     * @brief Removes @p ptr from the list of active writers.
     */
    void stop_writer(continuable* ptr);

    /**
     * @brief Adds @p ptr to the list of active writers.
     */
    void continue_writer(continuable* ptr);

    /**
     * @brief Checks wheter @p ptr is an active writer.
     * @warning This member function is not thread-safe.
     */
    bool has_writer(continuable* ptr);

    /**
     * @brief Removes @p ptr from the list of active readers.
     * @warning This member function is not thread-safe.
     */
    void stop_reader(continuable* ptr);

    /**
     * @brief Adds @p ptr to the list of active readers.
     * @warning This member function is not thread-safe.
     */
    void continue_reader(continuable* ptr);

    /**
     * @brief Checks wheter @p ptr is an active reader.
     * @warning This member function is not thread-safe.
     */
    bool has_reader(continuable* ptr);

    /**
     * @brief Tries to register a new peer, i.e., a new node in the network.
     *        Returns false if there is already a connection to @p node,
     *        otherwise true.
     */
    virtual bool register_peer(const node_id& node, peer* ptr) = 0;

    /**
     * @brief Returns the peer associated with given node id.
     */
    virtual peer* get_peer(const node_id& node) = 0;

    /**
     * @brief This callback is used by peer_acceptor implementations to
     *        invoke cleanup code when disposed.
     */
    virtual void del_acceptor(peer_acceptor* ptr) = 0;

    /**
     * @brief This callback is used by peer implementations to
     *        invoke cleanup code when disposed.
     */
    virtual void del_peer(peer* ptr) = 0;

    /**
     * @brief Delivers a message to given node.
     */
    virtual void deliver(const node_id& node,
                         const message_header& hdr,
                         any_tuple msg                  ) = 0;

    /**
     * @brief This callback is invoked by {@link peer} implementations
     *        and causes the middleman to disconnect from the node.
     */
    virtual void last_proxy_exited(peer* ptr) = 0;

    /**
     *
     */
    virtual void new_peer(const input_stream_ptr& in,
                          const output_stream_ptr& out,
                          const node_id_ptr& node = nullptr) = 0;

    /**
     * @brief Adds a new acceptor for incoming connections to @p pa
     *        to the event loop of the middleman.
     * @note This member function is thread-safe.
     */
    virtual void register_acceptor(const actor_addr& pa,
                                   peer_acceptor* ptr) = 0;

    /**
     * @brief Returns the namespace that contains all remote actors
     *        connected to this middleman.
     */
    inline actor_namespace& get_namespace();

 protected:

    // creates a middleman instance
    static middleman* create_singleton();

    // destroys uninitialized instances
    inline void dispose() { delete this; }

    // destroys an initialized singleton
    virtual void destroy() = 0;

    // initializes a singleton
    virtual void initialize() = 0;

    // each middleman defines its own namespace
    actor_namespace m_namespace;

    // the node id of this middleman
    node_id_ptr m_node;

    std::unique_ptr<middleman_event_handler> m_handler;

};

int dumb_socketpair(native_socket_type socks[2], int make_overlapped);

inline actor_namespace& middleman::get_namespace() {
    return m_namespace;
}

} } // namespace cppa::io

#endif // MIDDLEMAN_HPP
