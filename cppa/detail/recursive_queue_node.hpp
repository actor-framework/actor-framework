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

#include "cppa/actor.hpp"
#include "cppa/any_tuple.hpp"

namespace cppa { namespace detail {

struct recursive_queue_node {
    recursive_queue_node* next; // intrusive next pointer
    bool marked;                // denotes if this node is currently processed
    actor_ptr sender;
    any_tuple msg;

    inline recursive_queue_node() : next(nullptr), marked(false) {
    }

    inline recursive_queue_node(actor* from, any_tuple&& content)
        : next(nullptr)
        , marked(false)
        , sender(from)
        , msg(std::move(content)) {
    }

    recursive_queue_node(recursive_queue_node&&) = delete;
    recursive_queue_node(const recursive_queue_node&) = delete;
    recursive_queue_node& operator=(recursive_queue_node&&) = delete;
    recursive_queue_node& operator=(const recursive_queue_node&) = delete;

};

} } // namespace cppa::detail

#endif // CPPA_RECURSIVE_QUEUE_NODE_HPP
