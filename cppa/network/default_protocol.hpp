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


#ifndef DEFAULT_PROTOCOL_HPP
#define DEFAULT_PROTOCOL_HPP

#include <map>
#include <vector>

#include "cppa/actor_addressing.hpp"
#include "cppa/process_information.hpp"

#include "cppa/network/protocol.hpp"
#include "cppa/network/default_peer.hpp"
#include "cppa/network/default_peer_acceptor.hpp"
#include "cppa/network/default_message_queue.hpp"
#include "cppa/network/default_actor_addressing.hpp"

namespace cppa { namespace network {

class default_protocol : public protocol {

    typedef protocol super;

 public:

    default_protocol(abstract_middleman* parent);

    atom_value identifier() const;

    void publish(const actor_ptr& whom, variant_args args);

    void publish(const actor_ptr& whom,
                 std::unique_ptr<acceptor> acceptor,
                 variant_args args                  );

    void unpublish(const actor_ptr& whom);

    actor_ptr remote_actor(variant_args args);

    actor_ptr remote_actor(io_stream_ptr_pair ioptrs, variant_args args);

    void register_peer(const process_information& node, default_peer* ptr);

    default_peer_ptr get_peer(const process_information& node);

    void enqueue(const process_information& node,
                 const message_header& hdr,
                 any_tuple msg);

    void new_peer(const input_stream_ptr& in,
                  const output_stream_ptr& out,
                  const process_information_ptr& node = nullptr);

    void last_proxy_exited(const default_peer_ptr& pptr);

    void continue_writer(const default_peer_ptr& pptr);

    // covariant return type
    default_actor_addressing* addressing();

 private:

    struct peer_entry {
        default_peer_ptr impl;
        default_message_queue_ptr queue;
    };

    default_actor_addressing m_addressing;
    std::map<actor_ptr,std::vector<default_peer_acceptor_ptr> > m_acceptors;
    std::map<process_information,peer_entry> m_peers;

};

typedef intrusive_ptr<default_protocol> default_protocol_ptr;

} } // namespace cppa::network

#endif // DEFAULT_PROTOCOL_HPP
