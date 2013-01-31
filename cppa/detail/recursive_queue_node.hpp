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
#include "cppa/message_id.hpp"
#include "cppa/ref_counted.hpp"
#include "cppa/detail/memory.hpp"

// needs access to constructor + destructor to initialize m_dummy_node
namespace cppa { class local_actor; }

namespace cppa { namespace detail {

class recursive_queue_node : public memory_cached_mixin<memory_managed> {

    friend class memory;
    friend class ::cppa::local_actor;

 public:

    typedef recursive_queue_node* pointer;

    pointer      next;   // intrusive next pointer
    bool         marked; // denotes if this node is currently processed
    actor_ptr    sender; // points to the sender of msg
    any_tuple    msg;    // 'content field'
    message_id_t mid;

    recursive_queue_node(recursive_queue_node&&) = delete;
    recursive_queue_node(const recursive_queue_node&) = delete;
    recursive_queue_node& operator=(recursive_queue_node&&) = delete;
    recursive_queue_node& operator=(const recursive_queue_node&) = delete;

 private:

    recursive_queue_node() = default;

    recursive_queue_node(actor_ptr sptr, any_tuple data, message_id_t id = message_id_t());

};

} } // namespace cppa::detail

#endif // CPPA_RECURSIVE_QUEUE_NODE_HPP
