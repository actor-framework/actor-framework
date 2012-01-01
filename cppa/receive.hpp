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


#ifndef RECEIVE_HPP
#define RECEIVE_HPP

#include "cppa/self.hpp"
#include "cppa/local_actor.hpp"

namespace cppa {

/**
 * @brief Dequeues the next message from the mailbox that's matched
 *        by @p rules and executes the corresponding callback.
 */
inline void receive(invoke_rules& rules)
{
    self->dequeue(rules);
}

inline void receive(timed_invoke_rules& rules)
{
    self->dequeue(rules);
}

inline void receive(timed_invoke_rules&& rules)
{
    timed_invoke_rules tmp(std::move(rules));
    self->dequeue(tmp);
}

inline void receive(invoke_rules&& rules)
{
    invoke_rules tmp(std::move(rules));
    self->dequeue(tmp);
}

template<typename Head, typename... Tail>
void receive(invoke_rules&& rules, Head&& head, Tail&&... tail)
{
    invoke_rules tmp(std::move(rules));
    receive(tmp.splice(std::forward<Head>(head)),
            std::forward<Tail>(tail)...);
}

template<typename Head, typename... Tail>
void receive(invoke_rules& rules, Head&& head, Tail&&... tail)
{
    receive(rules.splice(std::forward<Head>(head)),
            std::forward<Tail>(tail)...);
}

} // namespace cppa

#endif // RECEIVE_HPP
