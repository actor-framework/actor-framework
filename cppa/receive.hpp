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
#include "cppa/behavior.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/detail/receive_loop_helper.hpp"

namespace cppa {

#ifdef CPPA_DOCUMENTATION

/**
 * @brief Dequeues the next message from the mailbox that's matched
 *        by @p bhvr and executes the corresponding callback.
 * @param bhvr Denotes the actor's response the next incoming message.
 */
void receive(behavior& bhvr);

/**
 * @brief Receives messages in an endless loop.
 *
 * Semantically equal to: <tt>for (;;) { receive(bhvr); }</tt>
 * @param bhvr Denotes the actor's response the next incoming message.
 */
void receive_loop(behavior& bhvr);

/**
 * @brief Receives messages as long as @p stmt returns true.
 *
 * Semantically equal to: <tt>while (stmt()) { receive(...); }</tt>.
 *
 * <b>Usage example:</b>
 * @code
 * int i = 0;
 * receive_while([&]() { return (++i <= 10); })
 * (
 *     on<int>() >> int_fun,
 *     on<float>() >> float_fun
 * );
 * @endcode
 * @param stmt Lambda expression, functor or function returning a @c bool.
 * @returns A functor implementing the loop.
 */
template<typename Statement>
auto receive_while(Statement&& stmt);

/**
 * @brief Receives messages until @p stmt returns true.
 *
 * Semantically equal to: <tt>do { receive(...); } while (stmt() == false);</tt>
 *
 * <b>Usage example:</b>
 * @code
 * int i = 0;
 * do_receive
 * (
 *     on<int>() >> int_fun,
 *     on<float>() >> float_fun
 * )
 * .until([&]() { return (++i >= 10); };
 * @endcode
 * @param bhvr Denotes the actor's response the next incoming message.
 * @returns A functor providing the @c until member function.
 */
auto do_receive(behavior& bhvr);

#else // CPPA_DOCUMENTATION

inline void receive(invoke_rules& bhvr) { self->dequeue(bhvr); }

inline void receive(timed_invoke_rules& bhvr) { self->dequeue(bhvr); }

inline void receive(behavior& bhvr)
{
    if (bhvr.is_left()) self->dequeue(bhvr.left());
    else self->dequeue(bhvr.right());
}

inline void receive(timed_invoke_rules&& bhvr)
{
    timed_invoke_rules tmp(std::move(bhvr));
    self->dequeue(tmp);
}

inline void receive(invoke_rules&& bhvr)
{
    invoke_rules tmp(std::move(bhvr));
    self->dequeue(tmp);
}

template<typename Head, typename... Tail>
void receive(invoke_rules&& bhvr, Head&& head, Tail&&... tail)
{
    invoke_rules tmp(std::move(bhvr));
    receive(tmp.splice(std::forward<Head>(head)),
            std::forward<Tail>(tail)...);
}

template<typename Head, typename... Tail>
void receive(invoke_rules& bhvr, Head&& head, Tail&&... tail)
{
    receive(bhvr.splice(std::forward<Head>(head)),
            std::forward<Tail>(tail)...);
}


void receive_loop(invoke_rules& rules);

void receive_loop(timed_invoke_rules& rules);

inline void receive_loop(behavior& bhvr)
{
    if (bhvr.is_left()) receive_loop(bhvr.left());
    else receive_loop(bhvr.right());
}

inline void receive_loop(invoke_rules&& rules)
{
    invoke_rules tmp(std::move(rules));
    receive_loop(tmp);
}

inline void receive_loop(timed_invoke_rules&& rules)
{
    timed_invoke_rules tmp(std::move(rules));
    receive_loop(tmp);
}

template<typename Head, typename... Tail>
void receive_loop(invoke_rules& rules, Head&& head, Tail&&... tail)
{
    receive_loop(rules.splice(std::forward<Head>(head)),
                 std::forward<Tail>(tail)...);
}

template<typename Head, typename... Tail>
void receive_loop(invoke_rules&& rules, Head&& head, Tail&&... tail)
{
    invoke_rules tmp(std::move(rules));
    receive_loop(tmp.splice(std::forward<Head>(head)),
                 std::forward<Tail>(tail)...);
}

template<typename Statement>
detail::receive_while_helper<Statement>
receive_while(Statement&& stmt)
{
    static_assert(std::is_same<bool, decltype(stmt())>::value,
                  "functor or function does not return a boolean");
    return std::move(stmt);
}

template<typename... Args>
detail::do_receive_helper do_receive(Args&&... args)
{
    return detail::do_receive_helper(std::forward<Args>(args)...);
}

#endif // CPPA_DOCUMENTATION

} // namespace cppa

#endif // RECEIVE_HPP
