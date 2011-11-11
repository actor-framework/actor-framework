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
#include "cppa/local_actor.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/announce.hpp"
#include "cppa/scheduler.hpp"
#include "cppa/to_string.hpp"
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
 * @author Dominik Charousset <dominik.charousset (at) haw-hamburg.de>
 *
 * @mainpage libcppa
 *
 * @section Intro Introduction
 *
 * This library provides an implementation of the Actor model for C++.
 *
 * @section GettingStarted Getting started with libcppa
 *
 * @namespace cppa
 * @brief This is the root namespace of libcppa.
 *
 * Thie @b cppa namespace contains all functions and classes to
 * implement Actor based applications.
 *
 * @namespace cppa::util
 * @brief This namespace contains utility classes and meta programming
 *        utilities used by the libcppa implementation.
 *
 * @namespace cppa::detail
 * @brief This namespace contains implementation details. Classes and
 *        functions of this namespace could change even in minor
 *        updates of the library and should not be used by outside of libcppa.
 *
 * @defgroup CopyOnWrite Copy-on-write optimization.
 * @p libcppa uses a copy-on-write optimization for its message
 * passing implementation.
 *
 * {@link tuple Tuples} should @b always be used used with by-value semantic,
 * since tuples use a copy-on-write smart pointer internally. Let's assume two
 * tuple @p x and @p y, whereas @p y is a copy of @p x:
 *
 * @code
 * auto x = make_tuple(1, 2, 3);
 * auto y = x;
 * @endcode
 *
 * Those two tuples initially point to the same data (the addresses of the
 * first element of @p x is equal to the address of the first element
 * of @p y):
 *
 * @code
 * assert(&(get<0>(x)) == &(get<0>(y)));
 * @endcode
 *
 * <tt>get<0>(x)</tt> returns a const-reference to the first element of @p x.
 * The function @p get does not have a const-overload to avoid
 * unintended copies. The function @p get_ref could be used to
 * modify tuple elements. A call to this function detaches
 * the tuple by copying the data before modifying it if there are two or more
 * references to the data:
 *
 * @code
 * // detaches x from y
 * get_ref<0>(x) = 42;
 * // x and y no longer point to the same data
 * assert(&(get<0>(x)) != &(get<0>(y)));
 * @endcode
 *
 * @defgroup MessageHandling Send and receive messages
 *
 * @section UsingOwnTypes Using own types in messages
 *
 * @defgroup ImplicitConversion Implicit type conversions.
 *
 * The message passing of @p libcppa prohibits pointers in messages because
 * it enforces network transparent messaging.
 * Unfortunately, string literals in @p C++ have the type <tt>const char*</tt>,
 * resp. <tt>const char[]</tt>. Since @p libcppa is a user-friendly library,
 * it silently converts string literals and C-strings to @p std::string objects.
 * It also converts unicode literals to the corresponding STL container.
 *
 * A few examples:
 * @code
 * // sends an std::string containing "hello actor!" to itself
 * send(self(), "hello actor!");
 *
 * const char* cstring = "cstring";
 * // sends an std::string containing "cstring" to itself
 * send(self(), cstring);
 *
 * // sends an std::u16string containing the UTF16 string "hello unicode world!"
 * send(self(), u"hello unicode world!");
 *
 * // x has the type tuple<std::string, std::string>
 * auto x = make_tuple("hello", "tuple");
 *
 * @endcode
 */


namespace cppa {

/**
 * @brief Links the calling actor to @p other.
 * @param other An Actor that is related with the calling Actor.
 */
void link(actor_ptr& other);

/**
 * @copydoc link(actor_ptr&)
 * Support for rvalue references.
 */
void link(actor_ptr&& other);

/**
 * @brief Links @p lhs and @p rhs;
 * @param lhs First Actor instance.
 * @param rhs Second Actor instance.
 * @pre <tt>lhs != rhs</tt>
 */
void link(actor_ptr&  lhs, actor_ptr&  rhs);

/**
 * @copydoc link(actor_ptr&,actor_ptr&)
 * Support for rvalue references.
 */
void link(actor_ptr&& lhs, actor_ptr&  rhs);

/**
 * @copydoc link(actor_ptr&,actor_ptr&)
 * Support for rvalue references.
 */
void link(actor_ptr&& lhs, actor_ptr&& rhs);

/**
 * @copydoc link(actor_ptr&,actor_ptr&)
 * Support for rvalue references.
 */
void link(actor_ptr&  lhs, actor_ptr&& rhs);

/**
 * @brief Unlinks @p lhs and @p rhs;
 */
void unlink(actor_ptr& lhs, actor_ptr& rhs);

/**
 * @brief Adds a monitor to @p whom.
 *
 * Sends a ":Down" message to the calling Actor if @p whom exited.
 * @param whom Actor instance that should be monitored by the calling Actor.
 */
void monitor(actor_ptr& whom);

/**
 * @copydoc monitor(actor_ptr&)
 * Support for rvalue references.
 */
void monitor(actor_ptr&& whom);

/**
 * @brief Removes a monitor from @p whom.
 * @param whom Monitored Actor.
 */
void demonitor(actor_ptr& whom);

/**
 * @brief Gets the @c trap_exit state of the calling Actor.
 * @returns @c true if the Actor explicitly handles Exit messages;
 *          @c false if the Actor uses the default behavior (finish execution
 *          if an Exit message with non-normal exit reason was received).
 */
inline bool trap_exit()
{
    return self()->trap_exit();
}

/**
 * @brief Sets the @c trap_exit state of the calling Actor.
 * @param new_value  Set this to @c true if you want to explicitly handle Exit
 *                   messages. The default is @c false, causing an Actor to
 *                   finish execution if an Exit message with non-normal
 *                   exit reason was received.
 */
inline void trap_exit(bool new_value)
{
    self()->trap_exit(new_value);
}

/**
 * @brief Spawns a new actor that executes @p what with given arguments.
 * @param Hint Hint to the scheduler for the best scheduling strategy.
 * @param what Function or functor that the spawned Actor should execute.
 * @param args Arguments needed to invoke @p what.
 * @returns A pointer to the newly created {@link actor Actor}.
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
 * @param what Function or functor that the spawned Actor should execute.
 * @param args Arguments needed to invoke @p what.
 * @returns A pointer to the newly created {@link actor Actor}.
 */
template<typename F, typename... Args>
inline actor_ptr spawn(F&& what, const Args&... args)
{
    return spawn<scheduled>(std::forward<F>(what), args...);
}

/**
 * @copydoc context::quit(std::uint32_t)
 *
 * Alias for <tt>self()->quit(reason);</tt>
 */
inline void quit(std::uint32_t reason)
{
    self()->quit(reason);
}

/**
 * @brief Receives messages in an endless loop.
 *
 * Semantically equal to: <tt>for (;;) { receive(rules); }</tt>
 * @param rules Invoke rules to receive and handle messages.
 */
void receive_loop(invoke_rules& rules);

/**
 * @copydoc receive_loop(invoke_rules&)
 * Support for invoke rules with timeout.
 */
void receive_loop(timed_invoke_rules& rules);

/**
 * @copydoc receive_loop(invoke_rules&)
 * Support for rvalue references.
 */
inline void receive_loop(invoke_rules&& rules)
{
    invoke_rules tmp(std::move(rules));
    receive_loop(tmp);
}

/**
 * @copydoc receive_loop(invoke_rules&)
 * Support for rvalue references and timeout.
 */
inline void receive_loop(timed_invoke_rules&& rules)
{
    timed_invoke_rules tmp(std::move(rules));
    receive_loop(tmp);
}

/**
 * @brief Receives messages in an endless loop.
 *
 * This function overload provides a simple way to define a receive loop
 * with on-the-fly {@link invoke_rules}.
 *
 * @b Example:
 * @code
 * receive_loop(on<int>() >> int_fun, on<float>() >> float_fun);
 * @endcode
 * @see receive_loop(invoke_rules&)
 */
template<typename Head, typename... Tail>
void receive_loop(invoke_rules& rules, Head&& head, Tail&&... tail)
{
    receive_loop(rules.splice(std::forward<Head>(head)),
                 std::forward<Tail>(tail)...);
}

/**
 * @brief Receives messages in an endless loop.
 *
 * This function overload provides a simple way to define a receive loop
 * with on-the-fly {@link invoke_rules}.
 *
 * @b Example:
 * @code
 * receive_loop(on<int>() >> int_fun, on<float>() >> float_fun);
 * @endcode
 * @see receive_loop(invoke_rules&)
 *
 * Support for rvalue references.
 */
template<typename Head, typename... Tail>
void receive_loop(invoke_rules&& rules, Head&& head, Tail&&... tail)
{
    invoke_rules tmp(std::move(rules));
    receive_loop(tmp.splice(std::forward<Head>(head)),
                 std::forward<Tail>(tail)...);
}

/**
 * @brief Receives messages as long as @p stmt returns true.
 *
 * Semantically equal to: <tt>while (stmt()) { receive(...); }</tt>.
 *
 * <b>Usage example:</b>
 * @code
 * int i = 0;
 * receive_while([&]() { return (++i < 10); })
 * (
 *     on<int>() >> int_fun,
 *     on<float>() >> float_fun
 * );
 * @endcode
 * @param stmt Lambda expression, functor or function returning a @c bool.
 * @returns A functor that takes invoke rules.
 */
template<typename Statement>
detail::receive_while_helper<Statement>
receive_while(Statement&& stmt)
{
    static_assert(std::is_same<bool, decltype(stmt())>::value,
                  "functor or function does not return a boolean");
    return std::move(stmt);
}

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
 * @param args Invoke rules to handle received messages.
 * @returns A functor providing the @c until function.
 */
template<typename... Args>
detail::do_receive_helper do_receive(Args&&... args)
{
    return detail::do_receive_helper(std::forward<Args>(args)...);
}

/**
 * @brief Gets the last dequeued message from the mailbox.
 * @returns The last dequeued message from the mailbox.
 */
inline any_tuple& last_received()
{
    return self()->mailbox().last_dequeued();
}

/**
 * @brief Gets the sender of the last received message.
 * @returns An {@link actor_ptr} to the sender of the last received message.
 */
inline actor_ptr& last_sender()
{
    return self()->mailbox().last_sender();
}

#ifdef CPPA_DOCUMENTATION

/**
 * @brief Send a message to @p whom.
 *
 * Sends the tuple <tt>{ arg0, args... }</tt> as a message to @p whom.
 * @param whom Receiver of the message.
 * @param arg0 First value for the message content.
 * @param args Any number of values for the message content.
 */
template<typename Arg0, typename... Args>
void send(channel_ptr& whom, const Arg0& arg0, const Args&... args);

/**
 * @brief Send a message to @p whom.
 *
 * <b>Usage example:</b>
 * @code
 * self() << make_tuple(1, 2, 3);
 * @endcode
 *
 * Sends the tuple @what as a message to @p whom.
 * @param whom Receiver of the message.
 * @param what Content of the message.
 */
channel_ptr& operator<<(channel_ptr& whom, const any_tuple& what);

#else

template<class C, typename Arg0, typename... Args>
typename util::enable_if<std::is_base_of<channel, C>, void>::type
send(intrusive_ptr<C>& whom, const Arg0& arg0, const Args&... args)
{
    if (whom) whom->enqueue(self(), make_tuple(arg0, args...));
}

template<class C, typename Arg0, typename... Args>
typename util::enable_if<std::is_base_of<channel, C>, void>::type
send(intrusive_ptr<C>&& whom, const Arg0& arg0, const Args&... args)
{
    intrusive_ptr<C> tmp(std::move(whom));
    if (tmp) tmp->enqueue(self(), make_tuple(arg0, args...));
}

// matches send(self(), ...);
template<typename Arg0, typename... Args>
void send(local_actor* whom, const Arg0& arg0, const Args&... args)
{
    if (whom) whom->enqueue(self(), make_tuple(arg0, args...));
}

template<class C>
typename util::enable_if<std::is_base_of<channel, C>, void>::type
send_tuple(intrusive_ptr<C>& whom, const any_tuple& what)
{
    if (whom) whom->enqueue(self(), what);
}

template<class C>
typename util::enable_if<std::is_base_of<channel, C>, void>::type
send_tuple(intrusive_ptr<C>&& whom, const any_tuple& what)
{
    intrusive_ptr<C> tmp(std::move(whom));
    if (tmp) tmp->enqueue(self(), what);
}

// matches send(self(), ...);
inline void send_tuple(local_actor* whom, const any_tuple& what)
{
    if (whom) whom->enqueue(self(), what);
}

template<class C>
typename util::enable_if<std::is_base_of<channel, C>, intrusive_ptr<C>&>::type
operator<<(intrusive_ptr<C>& whom, const any_tuple& what)
{
    if (whom) whom->enqueue(self(), what);
    return whom;
}

template<class C>
typename util::enable_if<std::is_base_of<channel, C>, intrusive_ptr<C>>::type
operator<<(intrusive_ptr<C>&& whom, const any_tuple& what)
{
    intrusive_ptr<C> tmp(std::move(whom));
    if (tmp) tmp->enqueue(self(), what);
    return std::move(tmp);
}

template<class C>
typename util::enable_if<std::is_base_of<channel, C>, intrusive_ptr<C>&>::type
operator<<(intrusive_ptr<C>& whom, any_tuple&& what)
{
    if (whom) whom->enqueue(self(), std::move(what));
    return whom;
}

template<class C>
typename util::enable_if<std::is_base_of<channel, C>, intrusive_ptr<C>>::type
operator<<(intrusive_ptr<C>&& whom, any_tuple&& what)
{
    intrusive_ptr<C> tmp(std::move(whom));
    if (tmp) tmp->enqueue(self(), std::move(what));
    return std::move(tmp);
}

// matches self() << make_tuple(...)
local_actor* operator<<(local_actor* whom, const any_tuple& what);

// matches self() << make_tuple(...)
local_actor* operator<<(local_actor* whom, any_tuple&& what);

#endif

template<class C, typename Arg0, typename... Args>
void reply(const Arg0& arg0, const Args&... args)
{
    send(self()->mailbox().last_sender(), arg0, args...);
}

inline void reply_tuple(const any_tuple& what)
{
    send_tuple(self()->mailbox().last_sender(), what);
}

/**
 * @brief Sends a message to @p whom that is delayed by @p rel_time.
 * @param whom Receiver of the message.
 * @param rel_time Relative time duration to delay the message.
 * @param data Any number of values for the message content.
 */
template<typename Duration, typename... Data>
void future_send(actor_ptr whom, const Duration& rel_time, const Data&... data)
{
    get_scheduler()->future_send(whom, rel_time, data...);
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
 * @param whom Actor that should be published at given @p port.
 * @param port Unused TCP port.
 * @throws bind_failure
 */
void publish(actor_ptr& whom, std::uint16_t port);

/**
 * @copydoc publish(actor_ptr&,std::uint16_t)
 * Support for rvalue references.
 */
void publish(actor_ptr&& whom, std::uint16_t port);

/**
 * @brief Establish a new connection to the actor at @p host on given @p port.
 * @param host Valid hostname or IP address.
 * @param port TCP port.
 * @returns A pointer to the proxy instance that represents the remote Actor.
 */
actor_ptr remote_actor(const char* host, std::uint16_t port);

} // namespace cppa

#endif // CPPA_HPP
