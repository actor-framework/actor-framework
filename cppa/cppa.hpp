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
 * Copyright (C) 2011, Dominik Charousset <dominik.charousset@haw-hamburg.de> *
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

#ifndef CPPA_HPP
#define CPPA_HPP

#include <tuple>
#include <cstdint>
#include <type_traits>

#include "cppa/on.hpp"
#include "cppa/atom.hpp"
#include "cppa/tuple.hpp"
#include "cppa/actor.hpp"
#include "cppa/invoke.hpp"
#include "cppa/channel.hpp"
#include "cppa/context.hpp"
#include "cppa/message.hpp"
#include "cppa/scheduler.hpp"
#include "cppa/exit_reason.hpp"
#include "cppa/invoke_rules.hpp"
#include "cppa/actor_behavior.hpp"
#include "cppa/scheduling_hint.hpp"

#include "cppa/util/rm_ref.hpp"
#include "cppa/util/enable_if.hpp"
#include "cppa/util/disable_if.hpp"

#include "cppa/detail/get_behavior.hpp"
#include "cppa/detail/receive_loop_helper.hpp"

/**
 * @brief The root namespace of libcppa.
 */
namespace cppa {

/**
 * @brief Links the calling actor to @p other.
 */
void link(actor_ptr& other);

void link(actor_ptr&& other);

/**
 * @brief Links @p lhs and @p rhs;
 */
void link(actor_ptr&  lhs, actor_ptr&  rhs);

// enable link(spawn(...), spawn(...)), etc.
void link(actor_ptr&& lhs, actor_ptr&  rhs);
void link(actor_ptr&& lhs, actor_ptr&& rhs);
void link(actor_ptr&  lhs, actor_ptr&& rhs);

/**
 * @brief Unlinks @p lhs and @p rhs;
 */
void unlink(actor_ptr& lhs, actor_ptr& rhs);

/**
 * @brief Adds a monitor to @p whom.
 *
 * Sends a ":Down" message if @p whom exited.
 */
void monitor(actor_ptr& whom);

// enable monitor(spawn(...))
void monitor(actor_ptr&& whom);

/**
 * @brief Removes a monitor from @p whom.
 */
void demonitor(actor_ptr& whom);

inline void trap_exit(bool new_value)
{
    self()->trap_exit(new_value);
}

/**
 * @brief Spawns a new actor that executes @p what with given arguments.
 */
template<scheduling_hint Hint, typename F, typename... Args>
actor_ptr spawn(F&& what, const Args&... args)
{
    typedef typename util::rm_ref<F>::type ftype;
    std::integral_constant<bool, std::is_function<ftype>::value> is_fun;
    auto ptr = detail::get_behavior(is_fun, std::forward<F>(what), args...);
    return get_scheduler()->spawn(ptr, Hint);
}

/**
 * @brief Alias for <tt>spawn<scheduled>(what, args...)</tt>.
 */
template<typename F, typename... Args>
actor_ptr spawn(F&& what, const Args&... args)
{
    return spawn<scheduled>(std::forward<F>(what), args...);
}

/**
 * @brief Quits execution of the calling actor.
 */
inline void quit(std::uint32_t reason)
{
    self()->quit(reason);
}

/**
 * @brief Receives messages in an endless loop.
 *
 * Equal to:
 * @code
 * for (;;) { receive(rules); }
 * @endcode
 */
void receive_loop(invoke_rules& rules);

inline void receive_loop(invoke_rules&& rules)
{
    invoke_rules tmp(std::move(rules));
    receive_loop(tmp);
}

template<typename Head, typename... Tail>
void receive_loop(invoke_rules&& rules, Head&& head, Tail... tail)
{
    invoke_rules tmp(std::move(rules));
    receive_loop(tmp.splice(std::forward<Head>(head)), tail...);
}

template<typename Head, typename... Tail>
void receive_loop(invoke_rules& rules, Head&& head, Tail... tail)
{
    receive_loop(rules.splice(std::forward<Head>(head)), tail...);
}

/**
 * @brief Receives messages as long as @p stmt returns true.
 *
 * Equal to:
 * @code
 * while (stmt()) { receive(...); }
 * @endcode
 */
template<typename Statement>
detail::receive_loop_helper<Statement, detail::receive_while_loop<Statement> >
receive_while(Statement&& stmt)
{
    static_assert(std::is_same<bool, decltype(stmt())>::value,
                  "statement is not a functor / lambda expression");
    return std::move(stmt);
}

/**
 * @brief Receives messages until @p stmt returns true.
 *
 * Equal to:
 * @code
 * do { receive(...); } while (stmt() == false);
 * @endcode
 */
template<typename Statement>
detail::receive_loop_helper<Statement, detail::receive_until_loop<Statement> >
receive_until(Statement&& stmt)
{
    static_assert(std::is_same<bool, decltype(stmt())>::value,
                  "statement is not a functor / lambda expression");
    return std::move(stmt);
}

/**
 * @brief Dequeues the next message from the mailbox.
 */
inline const message& receive()
{
    return self()->mailbox().dequeue();
}

/**
 * @brief Dequeues the next message from the mailbox that's matched
 *        by @p rules and executes the corresponding callback.
 */
inline void receive(invoke_rules& rules)
{
    self()->mailbox().dequeue(rules);
}

inline void receive(invoke_rules&& rules)
{
    self()->mailbox().dequeue(rules);
}

/**
 * @brief Tries to dequeue the next message from the mailbox.
 * @return @p true if a messages was dequeued;
 *         @p false if the mailbox is empty
 */
inline bool try_receive(message& msg)
{
    return self()->mailbox().try_dequeue(msg);
}

/**
 * @brief Tries to dequeue the next message from the mailbox.
 * @return @p true if a messages was dequeued;
 *         @p false if the mailbox is empty
 */
inline bool try_receive(invoke_rules& rules)
{
    return self()->mailbox().try_dequeue(rules);
}

/**
 * @brief Gets the last dequeued message from the mailbox.
 * @return The last dequeued message from the mailbox.
 */
inline const message& last_received()
{
    return self()->mailbox().last_dequeued();
}

template<class C, typename Arg0, typename... Args>
typename util::enable_if<std::is_base_of<channel, C>, void>::type
send(intrusive_ptr<C>& whom, const Arg0& arg0, const Args&... args)
{
    if (whom) whom->enqueue(message(self(), whom, arg0, args...));
}

template<class C, typename Arg0, typename... Args>
typename util::enable_if<std::is_base_of<channel, C>, void>::type
send(intrusive_ptr<C>&& whom, const Arg0& arg0, const Args&... args)
{
    if (whom) whom->enqueue(message(self(), whom, arg0, args...));
}

// 'matches' send(self(), ...);
template<typename Arg0, typename... Args>
void send(context* whom, const Arg0& arg0, const Args&... args)
{
    if (whom) whom->enqueue(message(self(), whom, arg0, args...));
}

template<class C>
typename util::enable_if<std::is_base_of<channel, C>, intrusive_ptr<C>&>::type
operator<<(intrusive_ptr<C>& whom, const any_tuple& what)
{
    if (whom) whom->enqueue(message(self(), whom, what));
    return whom;
}

template<class C>
typename util::enable_if<std::is_base_of<channel, C>, intrusive_ptr<C>&&>::type
operator<<(intrusive_ptr<C>&& whom, const any_tuple& what)
{
    if (whom) whom->enqueue(message(self(), whom, what));
    return std::move(whom);
}

template<class C>
typename util::enable_if<std::is_base_of<channel, C>, intrusive_ptr<C>&>::type
operator<<(intrusive_ptr<C>& whom, any_tuple&& what)
{
    if (whom) whom->enqueue(message(self(), whom, std::move(what)));
    return whom;
}

template<class C>
typename util::enable_if<std::is_base_of<channel, C>, intrusive_ptr<C>&&>::type
operator<<(intrusive_ptr<C>&& whom, any_tuple&& what)
{
    if (whom) whom->enqueue(message(self(), whom, std::move(what)));
    return std::move(whom);
}

// matches self() << make_tuple(...)
context* operator<<(context* whom, const any_tuple& what);

// matches self() << make_tuple(...)
context* operator<<(context* whom, any_tuple&& what);

template<typename Arg0, typename... Args>
void reply(const Arg0& arg0, const Args&... args)
{
    context* sptr = self();
    actor_ptr whom = sptr->mailbox().last_dequeued().sender();
    if (whom) whom->enqueue(message(sptr, whom, arg0, args...));
}

/**
 * @brief Blocks execution of this actor until all
 *        other actors finished execution.
 * @warning This function will cause a deadlock if
 *          called from multiple actors.
 */
inline void await_all_others_done()
{
    get_scheduler()->await_others_done();
}

/**
 * @brief Publishes @p whom at given @p port.
 *
 * The connection is automatically closed if the lifetime of @p whom ends.
 */
void publish(actor_ptr& whom, std::uint16_t port);

void publish(actor_ptr&& whom, std::uint16_t port);

/**
 * @brief Establish a new connection to the actor at @p host on given @p port.
 */
actor_ptr remote_actor(const char* host, std::uint16_t port);

} // namespace cppa

#endif // CPPA_HPP
