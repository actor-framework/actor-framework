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


#ifndef DEFAULT_PROTOCOL_HPP
#define DEFAULT_PROTOCOL_HPP

#include <map>
#include <vector>

#include "cppa/actor_addressing.hpp"
#include "cppa/process_information.hpp"

#include "cppa/io/protocol.hpp"
#include "cppa/io/middleman.hpp"
#include "cppa/io/default_peer.hpp"
#include "cppa/io/default_peer_acceptor.hpp"
#include "cppa/io/default_message_queue.hpp"
#include "cppa/io/default_actor_addressing.hpp"

namespace cppa { namespace io {

class default_protocol : public protocol {

    typedef protocol super;

    friend class default_peer;
    friend class default_peer_acceptor;

 public:

    default_protocol(middleman* multiplexer);

    atom_value identifier() const;

    void publish(const actor_ptr& whom, variant_args args);

    void publish(const actor_ptr& whom,
                 std::unique_ptr<acceptor> acceptor,
                 variant_args args                  );

    void unpublish(const actor_ptr& whom);

    actor_ptr remote_actor(variant_args args);

    actor_ptr remote_actor(stream_ptr_pair ioptrs, variant_args args);

    void register_peer(const process_information& node, default_peer* ptr);

    default_peer* get_peer(const process_information& node);

    void enqueue(const process_information& node,
                 const message_header& hdr,
                 any_tuple msg);

    void new_peer(const input_stream_ptr& in,
                  const output_stream_ptr& out,
                  const process_information_ptr& node = nullptr);

    void last_proxy_exited(default_peer* pptr);

    void continue_writer(default_peer* pptr);

    // covariant return type
    default_actor_addressing* addressing();

 private:

    void del_acceptor(default_peer_acceptor* ptr);

    void del_peer(default_peer* ptr);

    struct peer_entry {
        default_peer* impl;
        default_message_queue_ptr queue;
    };

    default_actor_addressing m_addressing;
    std::map<actor_ptr, std::vector<default_peer_acceptor*>> m_acceptors;
    std::map<process_information, peer_entry> m_peers;

};

} } // namespace cppa::network

#endif // DEFAULT_PROTOCOL_HPP
