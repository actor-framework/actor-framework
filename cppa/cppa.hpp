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


#ifndef CPPA_HPP
#define CPPA_HPP

#include <tuple>
#include <cstdint>
#include <type_traits>

#include "cppa/on.hpp"
#include "cppa/atom.hpp"
#include "cppa/self.hpp"
#include "cppa/tuple.hpp"
#include "cppa/actor.hpp"
#include "cppa/channel.hpp"
#include "cppa/receive.hpp"
#include "cppa/behavior.hpp"
#include "cppa/announce.hpp"
#include "cppa/scheduler.hpp"
#include "cppa/to_string.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/fsm_actor.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/exit_reason.hpp"
#include "cppa/scheduled_actor.hpp"
#include "cppa/scheduling_hint.hpp"
#include "cppa/event_based_actor.hpp"

#include "cppa/util/rm_ref.hpp"
#include "cppa/util/enable_if.hpp"
#include "cppa/util/disable_if.hpp"

#include "cppa/detail/actor_count.hpp"
#include "cppa/detail/get_behavior.hpp"
#include "cppa/detail/receive_loop_helper.hpp"

/**
 * @author Dominik Charousset <dominik.charousset (at) haw-hamburg.de>
 *
 * @mainpage libcppa
 *
 * @section Intro Introduction
 *
 * This library provides an implementation of the actor model for C++.
 * It uses a network transparent messaging system to ease development
 * of both concurrent and distributed software using C++.
 *
 * @p libcppa uses a thread pool to schedule actors by default.
 * A scheduled actor should not call blocking functions.
 * Individual actors can be spawned (created) with a special flag to run in
 * an own thread if one needs to make use of blocking APIs.
 *
 * Writing applications in @p libcppa requires a minimum of gluecode.
 * You don't have to derive a particular class to implement an actor and
 * each context <i>is</i> an actor. Even main is implicitly
 * converted to an actor if needed, as the following example shows:
 *
 * It's recommended to read at least the
 * {@link MessageHandling message handling}
 * section of this documentation.
 *
 * @section GettingStarted Getting Started
 *
 * To build @p libcppa, you need <tt>GCC >= 4.6</tt>, @p Automake
 * and the <tt>Boost.Thread</tt> library.
 *
 * The usual build steps on Linux and Mac OS X are:
 *
 * - <tt>autoreconf -i</tt>
 * - <tt>./configure</tt>
 * - <tt>make</tt>
 * - <tt>make install</tt> (as root, optionally)
 *
 * Use <tt>./configure --help</tt> if the script doesn't auto-select
 * the correct @p GCC binary or doesn't find your <tt>Boost.Thread</tt>
 * installation.
 *
 * Windows is not supported yet, because MVSC++ doesn't implement the
 * C++11 features needed to compile @p libcppa.
 *
 * @section IntroHelloWorld Hello World Example
 *
 * @include hello_world_example.cpp
 *
 * @section IntroMoreExamples More Examples
 *
 * The {@link math_actor_example.cpp Math Actor Example} shows the usage
 * of {@link receive_loop} and {@link cppa::arg_match arg_match}.
 * The {@link dining_philosophers.cpp Dining Philosophers Example}
 * introduces event-based actors and includes a lot of <tt>libcppa</tt>
 * features.
 *
 * @namespace cppa
 * @brief This is the root namespace of libcppa.
 *
 * Thie @b cppa namespace contains all functions and classes to
 * implement Actor based applications.
 *
 * @namespace cppa::util
 * @brief This namespace contains utility classes and metaprogramming
 *        utilities used by the libcppa implementation.
 *
 * @namespace cppa::intrusive
 * @brief This namespace contains intrusive container implementations.
 *
 * @defgroup CopyOnWrite Copy-on-write optimization.
 * @p libcppa uses a copy-on-write optimization for its message
 * passing implementation.
 *
 * {@link cppa::tuple Tuples} should @b always be used with by-value semantic,
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
 * @defgroup MessageHandling Message handling.
 *
 * This is the beating heart of @p libcppa. Actor programming is all about
 * message handling.
 *
 * A message in @p libcppa is a n-tuple of values (with size >= 1). You can use
 * almost every type in a messages.
 *
 * @section Send Send messages
 *
 * The function @p send could be used to send a message to an actor.
 * The first argument is the receiver of the message followed by any number
 * of values. @p send creates a tuple from the given values and enqueues the
 * tuple to the receivers mailbox. Thus, send should @b not be used to send
 * a message to multiple receivers. You should use the @p enqueue
 * member function of the receivers instead as in the following example:
 *
 * @code
 * // spawn some actors
 * auto a1 = spawn(...);
 * auto a2 = spawn(...);
 * auto a3 = spawn(...);
 *
 * // send a message to a1
 * send(a1, atom("hello"), "hello a1!");
 *
 * // send a message to a1, a2 and a3
 * auto msg = make_tuple(atom("compute"), 1, 2, 3);
 * auto s = self; // cache self pointer
 * // note: this is more efficient then using send() three times because
 * //       send() would create a new tuple each time;
 * //       this safes both time and memory thanks to libcppa's copy-on-write
 * a1->enqueue(s, msg);
 * a2->enqueue(s, msg);
 * a3->enqueue(s, msg);
 *
 * // modify msg and send it again
 * // (msg becomes detached due to copy-on-write optimization)
 * get_ref<1>(msg) = 10; // msg is now { atom("compute"), 10, 2, 3 }
 * a1->enqueue(s, msg);
 * a2->enqueue(s, msg);
 * a3->enqueue(s, msg);
 * @endcode
 *
 * @section Receive Receive messages
 *
 * The function @p receive takes a @p behavior as argument. The behavior
 * is a list of { pattern >> callback } rules.
 *
 * @code
 * receive
 * (
 *     on<atom("hello"), std::string>() >> [](std::string const& msg)
 *     {
 *         cout << "received hello message: " << msg << endl;
 *     },
 *     on<atom("compute"), int, int, int>() >> [](int i0, int i1, int i2)
 *     {
 *         // send our result back to the sender of this messages
 *         reply(atom("result"), i0 + i1 + i2);
 *     }
 * );
 * @endcode
 *
 * The function @p on creates a pattern.
 * It provides two ways of defining patterns:
 * either by template parameters (prefixed by up to four atoms) or by arguments.
 * The first way matches for types only (exept for the prefixing atoms).
 * The second way compares values.
 * Use the template function @p val to match for the type only.
 *
 * This example is equivalent to the previous one but uses the second way
 * to define patterns:
 *
 * @code
 * receive
 * (
 *     on(atom("hello"), val<std::string>()) >> [](std::string const& msg)
 *     {
 *         cout << "received hello message: " << msg << endl;
 *     },
 *     on(atom("compute"), val<int>(), val<int>(), val<int>()>() >> [](int i0, int i1, int i2)
 *     {
 *         // send our result back to the sender of this messages
 *         reply(atom("result"), i0 + i1 + i2);
 *     }
 * );
 * @endcode
 *
 * @section Atoms Atoms
 *
 * Atoms are a nice way to add semantic informations to a message.
 * Assuming an actor wants to provide a "math sevice" for integers. It
 * could provide operations such as addition, subtraction, etc.
 * This operations all have two operands. Thus, the actor does not know
 * what operation the sender of a message wanted by receiving just two integers.
 *
 * Example actor:
 * @code
 * void math_actor()
 * {
 *     receive_loop
 *     (
 *         on<atom("plus"), int, int>() >> [](int a, int b)
 *         {
 *             reply(atom("result"), a + b);
 *         },
 *         on<atom("minus"), int, int>() >> [](int a, int b)
 *         {
 *             reply(atom("result"), a - b);
 *         }
 *     );
 * }
 * @endcode
 *
 * @section ReceiveLoops Receive loops
 *
 * Previous examples using @p receive create behaviors on-the-fly.
 * This is inefficient in a loop since the argument passed to receive
 * is created in each iteration again. It's possible to store the behavior
 * in a variable and pass that variable to receive. This fixes the issue
 * of re-creation each iteration but rips apart definition and usage.
 *
 * There are four convenience functions implementing receive loops to
 * declare behavior where it belongs without unnecessary
 * copies: @p receive_loop, @p receive_while, @p receive_for and @p do_receive.
 *
 * @p receive_loop is analogous to @p receive and loops "forever" (until the
 * actor finishes execution).
 *
 * @p receive_while creates a functor evaluating a lambda expression.
 * The loop continues until the given lambda returns @p false. A simple example:
 *
 * @code
 * // receive two integers
 * vector<int> received_values;
 * receive_while([&]() { return received_values.size() < 2; })
 * (
 *     on<int>() >> [](int value)
 *     {
 *         received_values.push_back(value);
 *     }
 * );
 * // ...
 * @endcode
 *
 * @p receive_for is a simple ranged-based loop:
 *
 * @code
 * std::vector<int> vec {1, 2, 3, 4};
 * auto i = vec.begin();
 * receive_for(i, vec.end())
 * (
 *     on(atom("get")) >> [&]() { reply(atom("result"), *i); }
 * );
 * @endcode
 *
 * @p do_receive returns a functor providing the function @p until that
 * takes a lambda expression. The loop continues until the given lambda
 * returns true. Example:
 *
 * @code
 * // receive ints until zero was received
 * vector<int> received_values;
 * do_receive
 * (
 *     on<int>() >> [](int value)
 *     {
 *         received_values.push_back(value);
 *     }
 * )
 * .until([&]() { return received_values.back() == 0 });
 * // ...
 * @endcode
 *
 * @section FutureSend Send delayed messages
 *
 * The function @p future_send provides a simple way to delay a message.
 * This is particularly useful for recurring events, e.g., periodical polling.
 * Usage example:
 *
 * @code
 * future_send(self, std::chrono::seconds(1), atom("poll"));
 * receive_loop
 * (
 *     // ...
 *     on<atom("poll")>() >> []()
 *     {
 *         // ... poll something ...
 *         // and do it again after 1sec
 *         future_send(self, std::chrono::seconds(1), atom("poll"));
 *     }
 * );
 * @endcode
 *
 * See also the {@link dancing_kirby.cpp dancing kirby example}.
 *
 * @defgroup ImplicitConversion Implicit type conversions.
 *
 * The message passing of @p libcppa prohibits pointers in messages because
 * it enforces network transparent messaging.
 * Unfortunately, string literals in @p C++ have the type <tt>char const*</tt>,
 * resp. <tt>const char[]</tt>. Since @p libcppa is a user-friendly library,
 * it silently converts string literals and C-strings to @p std::string objects.
 * It also converts unicode literals to the corresponding STL container.
 *
 * A few examples:
 * @code
 * // sends an std::string containing "hello actor!" to itself
 * send(self, "hello actor!");
 *
 * char const* cstring = "cstring";
 * // sends an std::string containing "cstring" to itself
 * send(self, cstring);
 *
 * // sends an std::u16string containing the UTF16 string "hello unicode world!"
 * send(self, u"hello unicode world!");
 *
 * // x has the type cppa::tuple<std::string, std::string>
 * auto x = make_tuple("hello", "tuple");
 *
 * receive
 * (
 *     // equal to: on(std::string("hello actor!"))
 *     on("hello actor!") >> []() { }
 * );
 * @endcode
 *
 * @defgroup ActorManagement Actor management.
 *
 */

// examples

/**
 * @brief A trivial example program.
 * @example hello_world_example.cpp
 */

/**
 * @brief Shows the usage of {@link cppa::atom atoms}
 *        and {@link cppa::arg_match arg_match}.
 * @example math_actor_example.cpp
 */

/**
 * @brief A simple example for a future_send based application.
 * @example dancing_kirby.cpp
 */

/**
 * @brief An event-based "Dining Philosophers" implementation.
 * @example dining_philosophers.cpp
 */

namespace cppa {

/**
 * @ingroup ActorManagement
 * @brief Links @p lhs and @p rhs;
 * @pre <tt>lhs != rhs</tt>
 */
void link(actor_ptr&  lhs, actor_ptr&  rhs);

/**
 * @copydoc link(actor_ptr&,actor_ptr&)
 */
void link(actor_ptr&& lhs, actor_ptr&  rhs);

/**
 * @copydoc link(actor_ptr&,actor_ptr&)
 */
void link(actor_ptr&& lhs, actor_ptr&& rhs);

/**
 * @copydoc link(actor_ptr&,actor_ptr&)
 */
void link(actor_ptr&  lhs, actor_ptr&& rhs);

/**
 * @ingroup ActorManagement
 * @brief Unlinks @p lhs from @p rhs;
 * @pre <tt>lhs != rhs</tt>
 */
void unlink(actor_ptr& lhs, actor_ptr& rhs);

/**
 * @ingroup ActorManagement
 * @brief Adds a unidirectional @p monitor to @p whom.
 * @note Each calls to @p monitor creates a new, independent monitor.
 * @pre The calling actor receives a ":Down" message from @p whom when
 *      it terminates.
 */
void monitor(actor_ptr& whom);

void monitor(actor_ptr&& whom);

/**
 * @ingroup ActorManagement
 * @brief Removes a monitor from @p whom.
 */
void demonitor(actor_ptr& whom);

/**
 * @ingroup ActorManagement
 * @brief Spans a new context-switching actor.
 * @returns A pointer to the spawned {@link actor Actor}.
 */
inline actor_ptr spawn(scheduled_actor* what)
{
    return get_scheduler()->spawn(what, scheduled);
}

/**
 * @ingroup ActorManagement
 * @brief Spans a new context-switching actor.
 * @tparam Hint Hint to the scheduler for the best scheduling strategy.
 * @returns A pointer to the spawned {@link actor Actor}.
 */
template<scheduling_hint Hint>
inline actor_ptr spawn(scheduled_actor* what)
{
    return get_scheduler()->spawn(what, Hint);
}

/**
 * @ingroup ActorManagement
 * @brief Spans a new event-based actor.
 * @returns A pointer to the spawned {@link actor Actor}.
 */
inline actor_ptr spawn(abstract_event_based_actor* what)
{
    return get_scheduler()->spawn(what);
}

/**
 * @ingroup ActorManagement
 * @brief Spawns a new actor that executes @p what with given arguments.
 * @tparam Hint Hint to the scheduler for the best scheduling strategy.
 * @param what Function or functor that the spawned Actor should execute.
 * @param args Arguments needed to invoke @p what.
 * @returns A pointer to the spawned {@link actor actor}.
 */
template<scheduling_hint Hint, typename F, typename... Args>
auto //actor_ptr
spawn(F&& what, Args const&... args)
-> typename util::disable_if_c<   std::is_convertible<typename util::rm_ref<F>::type, scheduled_actor*>::value
                               || std::is_convertible<typename util::rm_ref<F>::type, event_based_actor*>::value,
                               actor_ptr>::type
{
    typedef typename util::rm_ref<F>::type ftype;
    std::integral_constant<bool, std::is_function<ftype>::value> is_fun;
    auto ptr = detail::get_behavior(is_fun, std::forward<F>(what), args...);
    return get_scheduler()->spawn(ptr, Hint);
}

/**
 * @ingroup ActorManagement
 * @brief Alias for <tt>spawn<scheduled>(what, args...)</tt>.
 */
template<typename F, typename... Args>
auto // actor_ptr
spawn(F&& what, Args const&... args)
-> typename util::disable_if_c<   std::is_convertible<typename util::rm_ref<F>::type, scheduled_actor*>::value
                               || std::is_convertible<typename util::rm_ref<F>::type, event_based_actor*>::value,
                               actor_ptr>::type
{
    return spawn<scheduled>(std::forward<F>(what), args...);
}

#ifdef CPPA_DOCUMENTATION

/**
 * @ingroup MessageHandling
 * @brief Sends <tt>{arg0, args...}</tt> as a message to @p whom.
 */
template<typename Arg0, typename... Args>
void send(channel_ptr& whom, Arg0 const& arg0, Args const&... args);

/**
 * @ingroup MessageHandling
 * @brief Send a message to @p whom.
 *
 * <b>Usage example:</b>
 * @code
 * self << make_tuple(1, 2, 3);
 * @endcode
 * @returns @p whom.
 */
channel_ptr& operator<<(channel_ptr& whom, any_tuple const& what);

#else

template<class C, typename Arg0, typename... Args>
void send(intrusive_ptr<C>& whom, Arg0 const& arg0, Args const&... args)
{
    static_assert(std::is_base_of<channel, C>::value, "C is not a channel");
    if (whom) whom->enqueue(self, make_tuple(arg0, args...));
}

template<class C, typename Arg0, typename... Args>
void send(intrusive_ptr<C>&& whom, Arg0 const& arg0, Args const&... args)
{
    static_assert(std::is_base_of<channel, C>::value, "C is not a channel");
    intrusive_ptr<C> tmp(std::move(whom));
    if (tmp) tmp->enqueue(self, make_tuple(arg0, args...));
}

// matches "send(this, ...)" in event-based actors
template<typename Arg0, typename... Args>
void send(local_actor* whom, Arg0 const& arg0, Args const&... args)
{
    whom->enqueue(whom, make_tuple(arg0, args...));
}


// matches send(self, ...);
template<typename Arg0, typename... Args>
inline void send(self_type const&, Arg0 const& arg0, Args const&... args)
{
    send(static_cast<local_actor*>(self), arg0, args...);
}

template<class C>
typename util::enable_if<std::is_base_of<channel, C>, intrusive_ptr<C>&>::type
operator<<(intrusive_ptr<C>& whom, any_tuple const& what)
{
    if (whom) whom->enqueue(self, what);
    return whom;
}

template<class C>
typename util::enable_if<std::is_base_of<channel, C>, intrusive_ptr<C>>::type
operator<<(intrusive_ptr<C>&& whom, any_tuple const& what)
{
    intrusive_ptr<C> tmp(std::move(whom));
    if (tmp) tmp->enqueue(self, what);
    return std::move(tmp);
}

template<class C>
typename util::enable_if<std::is_base_of<channel, C>, intrusive_ptr<C>&>::type
operator<<(intrusive_ptr<C>& whom, any_tuple&& what)
{
    if (whom) whom->enqueue(self, std::move(what));
    return whom;
}

template<class C>
typename util::enable_if<std::is_base_of<channel, C>, intrusive_ptr<C>>::type
operator<<(intrusive_ptr<C>&& whom, any_tuple&& what)
{
    intrusive_ptr<C> tmp(std::move(whom));
    if (tmp) tmp->enqueue(self, std::move(what));
    return std::move(tmp);
}

self_type const& operator<<(self_type const& s, any_tuple const& what);

self_type const& operator<<(self_type const& s, any_tuple&& what);

#endif // CPPA_DOCUMENTATION

/**
 * @ingroup MessageHandling
 * @brief Sends a message to the sender of the last received message.
 */
template<typename Arg0, typename... Args>
void reply(Arg0 const& arg0, Args const&... args)
{
    send(self->last_sender(), arg0, args...);
}

/**
 * @ingroup MessageHandling
 * @brief Sends a message to @p whom that is delayed by @p rel_time.
 * @param whom Receiver of the message.
 * @param rel_time Relative time duration to delay the message.
 * @param data Any number of values for the message content.
 */
template<typename Duration, typename... Data>
void future_send(actor_ptr whom, Duration const& rel_time, Data const&... data)
{
    get_scheduler()->future_send(whom, rel_time, data...);
}

/**
 * @ingroup MessageHandling
 * @brief Sends a reply message that is delayed by @p rel_time.
 * @see future_send()
 */
template<typename Duration, typename... Data>
void delayed_reply(Duration const& rel_time, Data const... data)
{
    future_send(self->last_sender(), rel_time, data...);
}

/**
 * @brief Blocks execution of this actor until all
 *        other actors finished execution.
 * @warning This function will cause a deadlock if
 *          called from multiple actors.
 */
inline void await_all_others_done()
{
    detail::actor_count_wait_until((self.unchecked() == nullptr) ? 0 : 1);
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
 */
void publish(actor_ptr&& whom, std::uint16_t port);

/**
 * @brief Establish a new connection to the actor at @p host on given @p port.
 * @param host Valid hostname or IP address.
 * @param port TCP port.
 * @returns A pointer to the proxy instance that represents the remote Actor.
 */
actor_ptr remote_actor(char const* host, std::uint16_t port);

} // namespace cppa

#endif // CPPA_HPP
