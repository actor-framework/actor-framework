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


#ifndef CPPA_RECURSIVE_QUEUE_NODE_HPP
#define CPPA_RECURSIVE_QUEUE_NODE_HPP

#include <cstdint>

#include "cppa/actor.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/message_id.hpp"
#include "cppa/ref_counted.hpp"
#include "cppa/memory_cached.hpp"

// needs access to constructor + destructor to initialize m_dummy_node
namespace cppa { class local_actor; }

namespace cppa { namespace detail {

class recursive_queue_node : public memory_cached<memory_managed,recursive_queue_node> {

    friend class memory;
    friend class ::cppa::local_actor;

 public:

    typedef recursive_queue_node* pointer;

    pointer      next;   // intrusive next pointer
    bool         marked; // denotes if this node is currently processed
    actor_ptr    sender; // points to the sender of msg
    any_tuple    msg;    // 'content field'
    message_id mid;

    recursive_queue_node(recursive_queue_node&&) = delete;
    recursive_queue_node(const recursive_queue_node&) = delete;
    recursive_queue_node& operator=(recursive_queue_node&&) = delete;
    recursive_queue_node& operator=(const recursive_queue_node&) = delete;

    template<typename... Ts>
    inline static recursive_queue_node* create(Ts&&... args) {
        return memory::create<recursive_queue_node>(std::forward<Ts>(args)...);
    }

 private:

    recursive_queue_node() = default;

    recursive_queue_node(const actor_ptr& sptr,
                         any_tuple data,
                         message_id id = message_id{});

};

} } // namespace cppa::detail

#endif // CPPA_RECURSIVE_QUEUE_NODE_HPP
