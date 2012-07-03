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


#ifndef CPPA_RECURSIVE_QUEUE_NODE_HPP
#define CPPA_RECURSIVE_QUEUE_NODE_HPP

#include <cstdint>

#include "cppa/actor.hpp"
#include "cppa/any_tuple.hpp"

namespace cppa { namespace detail {

struct recursive_queue_node {

    typedef std::uint64_t seq_id_t;
    typedef recursive_queue_node* pointer;

    pointer   next;   // intrusive next pointer
    bool      marked; // denotes if this node is currently processed
    actor_ptr sender; // points to the sender of msg
    any_tuple msg;    // 'content field'
    seq_id_t  seq_id; // sequence id:
                      // - equals to 0 for asynchronous messages
                      // - first bit is 1 if this messages is a
                      //   response messages, otherwise 0
                      // - the trailing 63 bit uniquely identify a
                      //   request/response message pair

    inline recursive_queue_node() : next(nullptr), marked(false), seq_id(0) { }

    inline recursive_queue_node(actor* ptr, any_tuple&& data, seq_id_t id = 0)
    : next(nullptr), marked(false), sender(std::move(ptr))
    , msg(std::move(data)), seq_id(id) { }

    recursive_queue_node(recursive_queue_node&&) = delete;
    recursive_queue_node(const recursive_queue_node&) = delete;
    recursive_queue_node& operator=(recursive_queue_node&&) = delete;
    recursive_queue_node& operator=(const recursive_queue_node&) = delete;

};

} } // namespace cppa::detail

#endif // CPPA_RECURSIVE_QUEUE_NODE_HPP
