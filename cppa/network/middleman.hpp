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


#ifndef MIDDLEMAN_HPP
#define MIDDLEMAN_HPP

#include <map>
#include <vector>
#include <memory>

#include "cppa/network/peer.hpp"
#include "cppa/network/acceptor.hpp"
#include "cppa/network/peer_acceptor.hpp"
#include "cppa/network/continuable_reader.hpp"

namespace cppa { namespace detail { class singleton_manager; } }

namespace cppa { namespace network {

class middleman_impl;
class middleman_overseer;
class middleman_event_handler;

void middleman_loop(middleman_impl*);

class middleman {

    friend class peer;
    friend class peer_acceptor;
    friend class singleton_manager;
    friend class middleman_overseer;
    friend class middleman_event_handler;
    friend class detail::singleton_manager;

    friend void middleman_loop(middleman_impl*);

 public:

    virtual ~middleman();

    virtual void publish(std::unique_ptr<acceptor> server,
                         const actor_ptr& published_actor) = 0;

    virtual void add_peer(const io_stream_ptr_pair& io,
                          const process_information_ptr& node_info) = 0;

    virtual void unpublish(const actor_ptr& whom) = 0;

    virtual void enqueue(const process_information_ptr& receiving_node,
                         const addressed_message& message) = 0;

    inline void enqueue(const process_information_ptr& receiving_node,
                        actor_ptr sender,
                        channel_ptr receiver,
                        any_tuple msg,
                        message_id_t id = message_id_t()) {
        enqueue(receiving_node,
                addressed_message(std::move(sender),
                                  std::move(receiver),
                                  std::move(msg),
                                  id));
    }

 protected:

    middleman();

    virtual void stop() = 0;
    virtual void start() = 0;

 private:

    // to be called from singleton_manager

    static middleman* create_singleton();


    // to be called from peer

    void continue_writing_later(const peer_ptr& ptr);
    void register_peer(const process_information& node, const peer_ptr& ptr);


    // to be called form peer_acceptor

    void add(const continuable_reader_ptr& what);


    // to be called from m_handler or middleman_overseer

    inline void quit() { m_done = true; }
    inline bool done() const { return m_done; }
    void erase(const continuable_reader_ptr& what);
    continuable_reader_ptr acceptor_of(const actor_ptr& whom);
    peer_ptr get_peer(const process_information& node);


    // member variables

    bool m_done;
    std::vector<continuable_reader_ptr> m_readers;
    std::map<process_information,peer_ptr> m_peers;
    std::unique_ptr<middleman_event_handler> m_handler;

};

} } // namespace cppa::detail

#endif // MIDDLEMAN_HPP
