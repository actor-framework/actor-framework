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


#ifndef CPPA_SCOPED_ACTOR_HPP
#define CPPA_SCOPED_ACTOR_HPP

#include "cppa/behavior.hpp"
#include "cppa/thread_mapped_actor.hpp"

namespace cppa {
    
class scoped_actor {
    
public:
    
    /**
     * @brief Dequeues the next message from the mailbox that
     *        is matched by given behavior.
     */
    template<typename... Ts>
    void receive(Ts&&... args);
    
    /**
     * @brief Receives messages in an endless loop.
     *        Semantically equal to: <tt>for (;;) { receive(...); }</tt>
     */
    template<typename... Ts>
    void receive_loop(Ts&&... args);
    
    /**
     * @brief Receives messages as in a range-based loop.
     *
     * Semantically equal to:
     * <tt>for ( ; begin != end; ++begin) { receive(...); }</tt>.
     *
     * <b>Usage example:</b>
     * @code
     * int i = 0;
     * receive_for(i, 10) (
     *     on(atom("get")) >> [&]() -> any_tuple { return {"result", i}; }
     * );
     * @endcode
     * @param begin First value in range.
     * @param end Last value in range (excluded).
     * @returns A functor implementing the loop.
     */
    template<typename T>
    detail::receive_for_helper<T> receive_for(T& begin, const T& end);
    
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
    detail::receive_while_helper<Statement> receive_while(Statement&& stmt);
    
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
    template<typename... Ts>
    detail::do_receive_helper do_receive(Ts&&... args);
    
private:
    
    intrusive_ptr<thread_mapped_actor> m_self;
    
};

/******************************************************************************
 *                inline and template function implementations                *
 ******************************************************************************/

template<typename... Ts>
void scoped_actor::receive(Ts&&... args) {
    m_self->receive(std::forward<Ts(args)...);
}

template<typename... Ts>
void scoped_actor::receive_loop(Ts&&... args) {
    m_self->receive_loop(std::forward<Ts(args)...);
}

template<typename T>
detail::receive_for_helper<T> scoped_actor::receive_for(T& begin, const T& end) {
    m_self->receive_for(begin, end);
}

template<typename Statement>
detail::receive_while_helper<Statement> scoped_actor::receive_while(Statement&& stmt) {
    m_self->receive_while(std::forward<Statement(stmt));
}

template<typename... Ts>
detail::do_receive_helper scoped_actor::do_receive(Ts&&... args) {
    m_self->do_receive(std::forward<Ts(args)...);
}
    
} // namespace cppa

#endif // CPPA_SCOPED_ACTOR_HPP
