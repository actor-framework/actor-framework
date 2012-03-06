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

inline void receive(behavior& bhvr) { self->dequeue(bhvr); }

inline void receive(partial_function& fun) { self->dequeue(fun); }

inline void receive(behavior&& arg0)
{
    behavior tmp{std::move(arg0)};
    receive(tmp);
}

template<typename... Args>
void receive(partial_function&& arg0, Args&&... args)
{
    typename detail::select_bhvr<Args...>::type tmp;
    receive(tmp.splice(std::move(arg0), std::forward<Args>(args)...));
}

void receive_loop(behavior& rules);

void receive_loop(partial_function& rules);

inline void receive_loop(behavior&& arg0)
{
    behavior tmp{std::move(arg0)};
    receive_loop(tmp);
}

template<typename... Args>
void receive_loop(partial_function&& arg0, Args&&... args)
{
    typename detail::select_bhvr<Args...>::type tmp;
    receive_loop(tmp.splice(std::move(arg0), std::forward<Args>(args)...));
}

template<typename Statement>
detail::receive_while_helper<Statement> receive_while(Statement&& stmt)
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
