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


#ifndef CPPA_HPP
#define CPPA_HPP

#include <tuple>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <type_traits>

#include "cppa/on.hpp"
#include "cppa/atom.hpp"
#include "cppa/send.hpp"
#include "cppa/self.hpp"
#include "cppa/actor.hpp"
#include "cppa/match.hpp"
#include "cppa/channel.hpp"
#include "cppa/receive.hpp"
#include "cppa/factory.hpp"
#include "cppa/behavior.hpp"
#include "cppa/announce.hpp"
#include "cppa/sb_actor.hpp"
#include "cppa/threaded.hpp"
#include "cppa/scheduler.hpp"
#include "cppa/to_string.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/cow_tuple.hpp"
#include "cppa/tuple_cast.hpp"
#include "cppa/singletons.hpp"
#include "cppa/exit_reason.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/prioritizing.hpp"
#include "cppa/spawn_options.hpp"
#include "cppa/message_future.hpp"
#include "cppa/response_handle.hpp"
#include "cppa/scheduled_actor.hpp"
#include "cppa/event_based_actor.hpp"

#include "cppa/util/type_traits.hpp"
#include "cppa/io/acceptor.hpp"
#include "cppa/io/broker.hpp"
#include "cppa/io/middleman.hpp"
#include "cppa/io/io_handle.hpp"
#include "cppa/io/input_stream.hpp"
#include "cppa/io/output_stream.hpp"
#include "cppa/io/broker_backend.hpp"

#include "cppa/detail/memory.hpp"
#include "cppa/detail/get_behavior.hpp"
#include "cppa/detail/actor_registry.hpp"
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
 * of both concurrent and distributed software.
 *
 * @p libcppa uses a thread pool to schedule actors by default.
 * A scheduled actor should not call blocking functions.
 * Individual actors can be spawned (created) with a special flag to run in
 * an own thread if one needs to make use of blocking APIs.
 *
 * Writing applications in @p libcppa requires a minimum of gluecode and
 * each context <i>is</i> an actor. Even main is implicitly
 * converted to an actor if needed.
 *
 * @section GettingStarted Getting Started
 *
 * To build @p libcppa, you need <tt>GCC >= 4.7</tt> or <tt>Clang >= 3.2</tt>,
 * and @p CMake.
 *
 * The usual build steps on Linux and Mac OS X are:
 *
 *- <tt>mkdir build</tt>
 *- <tt>cd build</tt>
 *- <tt>cmake ..</tt>
 *- <tt>make</tt>
 *- <tt>make install</tt> (as root, optionally)
 *
 * Please run the unit tests as well to verify that @p libcppa works properly.
 *
 *- <tt>./bin/unit_tests</tt>
 *
 * Please submit a bug report that includes (a) your compiler version,
 * (b) your OS, and (c) the output of the unit tests if an error occurs.
 *
 * Windows is not supported yet, because MVSC++ doesn't implement the
 * C++11 features needed to compile @p libcppa.
 *
 * Please read the <b>Manual</b> for an introduction to @p libcppa.
 * It is available online at
 * http://neverlord.github.com/libcppa/manual/index.html or as PDF version at
 * http://neverlord.github.com/libcppa/manual/libcppa_manual.pdf
 *
 * @section IntroHelloWorld Hello World Example
 *
 * @include hello_world.cpp
 *
 * @section IntroMoreExamples More Examples
 *
 * The {@link math_actor.cpp Math Actor Example} shows the usage
 * of {@link receive_loop} and {@link cppa::arg_match arg_match}.
 * The {@link dining_philosophers.cpp Dining Philosophers Example}
 * introduces event-based actors and includes a lot of <tt>libcppa</tt>
 * features.
 *
 * @namespace cppa
 * @brief Root namespace of libcppa.
 *
 * @namespace cppa::util
 * @brief Contains utility classes and metaprogramming
 *        utilities used by the libcppa implementation.
 *
 * @namespace cppa::intrusive
 * @brief Contains intrusive container implementations.
 *
 * @namespace cppa::opencl
 * @brief Contains all classes of libcppa's OpenCL binding (optional).
 *
 * @namespace cppa::network
 * @brief Contains all network related classes.
 *
 * @namespace cppa::factory
 * @brief Contains factory functions to create actors from lambdas or
 *        other functors.
 *
 * @namespace cppa::exit_reason
 * @brief Contains all predefined exit reasons.
 *
 * @namespace cppa::placeholders
 * @brief Contains the guard placeholders @p _x1 to @p _x9.
 *
 * @defgroup CopyOnWrite Copy-on-write optimization.
 * @p libcppa uses a copy-on-write optimization for its message
 * passing implementation.
 *
 * {@link cppa::cow_tuple Tuples} should @b always be used with by-value
 * semantic, since tuples use a copy-on-write smart pointer internally.
 * Let's assume two
 * tuple @p x and @p y, whereas @p y is a copy of @p x:
 *
 * @code
 * auto x = make_cow_tuple(1, 2, 3);
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
 * @brief This is the beating heart of @p libcppa. Actor programming is
 *        all about message handling.
 *
 * A message in @p libcppa is a n-tuple of values (with size >= 1). You can use
 * almost every type in a messages - as long as it is announced, i.e., known
 * by libcppa's type system.
 *
 * @defgroup BlockingAPI Blocking API.
 *
 * @brief Blocking functions to receive messages.
 *
 * The blocking API of libcppa is intended to be used for migrating
 * previously threaded applications. When writing new code, you should use
 * ibcppas nonblocking become/unbecome API.
 *
 * @section Send Send messages
 *
 * The function @p send could be used to send a message to an actor.
 * The first argument is the receiver of the message followed by any number
 * of values. @p send creates a tuple from the given values and enqueues the
 * tuple to the receivers mailbox. Thus, send should @b not be used to send
 * a message to multiple receivers. You should use @p operator<<
 * instead as in the following example:
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
 * auto msg = make_cow_tuple(atom("compute"), 1, 2, 3);
 *
 * // note: this is more efficient then using send() three times because
 * //       send() would create a new tuple each time;
 * //       this safes both time and memory thanks to libcppa's copy-on-write
 * a1 << msg;
 * a2 << msg;
 * a3 << msg;
 *
 * // modify msg and send it again
 * // (msg becomes detached due to copy-on-write optimization)
 * get_ref<1>(msg) = 10; // msg is now { atom("compute"), 10, 2, 3 }
 * a1 << msg;
 * a2 << msg;
 * a3 << msg;
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
 *     on(atom("hello"), arg_match) >> [](const std::string& msg)
 *     {
 *         cout << "received hello message: " << msg << endl;
 *     },
 *     on(atom("compute"), arg_match) >> [](int i0, int i1, int i2)
 *     {
 *         // send our result back to the sender of this messages
 *         reply(atom("result"), i0 + i1 + i2);
 *     }
 * );
 * @endcode
 *
 * Please read the manual for further details about pattern matching.
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
 * void math_actor() {
 *     receive_loop (
 *         on(atom("plus"), arg_match) >> [](int a, int b) {
 *             reply(atom("result"), a + b);
 *         },
 *         on(atom("minus"), arg_match) >> [](int a, int b) {
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
 * receive_while([&]() { return received_values.size() < 2; }) (
 *     on<int>() >> [](int value) {
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
 * receive_for(i, vec.end()) (
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
 * do_receive (
 *     on<int>() >> [](int value) {
 *         received_values.push_back(value);
 *     }
 * )
 * .until([&]() { return received_values.back() == 0 });
 * // ...
 * @endcode
 *
 * @section FutureSend Send delayed messages
 *
 * The function @p delayed_send provides a simple way to delay a message.
 * This is particularly useful for recurring events, e.g., periodical polling.
 * Usage example:
 *
 * @code
 * delayed_send(self, std::chrono::seconds(1), atom("poll"));
 * receive_loop (
 *     // ...
 *     on(atom("poll")) >> [] {
 *         // ... poll something ...
 *         // and do it again after 1sec
 *         delayed_send(self, std::chrono::seconds(1), atom("poll"));
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
 * Unfortunately, string literals in @p C++ have the type <tt>const char*</tt>,
 * resp. <tt>const char[]</tt>. Since @p libcppa is a user-friendly library,
 * it silently converts string literals and C-strings to @p std::string objects.
 * It also converts unicode literals to the corresponding STL container.
 *
 * A few examples:
 * @code
 * // sends an std::string containing "hello actor!" to itself
 * send(self, "hello actor!");
 *
 * const char* cstring = "cstring";
 * // sends an std::string containing "cstring" to itself
 * send(self, cstring);
 *
 * // sends an std::u16string containing the UTF16 string "hello unicode world!"
 * send(self, u"hello unicode world!");
 *
 * // x has the type cppa::tuple<std::string, std::string>
 * auto x = make_cow_tuple("hello", "tuple");
 *
 * receive (
 *     // equal to: on(std::string("hello actor!"))
 *     on("hello actor!") >> [] { }
 * );
 * @endcode
 *
 * @defgroup ActorCreation Actor creation.
 *
 * @defgroup MetaProgramming Metaprogramming utility.
 */

// examples

/**
 * @brief A trivial example program.
 * @example hello_world.cpp
 */

/**
 * @brief Shows the usage of {@link cppa::atom atoms}
 *        and {@link cppa::arg_match arg_match}.
 * @example math_actor.cpp
 */

/**
 * @brief A simple example for a delayed_send based application.
 * @example dancing_kirby.cpp
 */

/**
 * @brief An event-based "Dining Philosophers" implementation.
 * @example dining_philosophers.cpp
 */

namespace cppa {

/**
 * @ingroup MessageHandling
 * @{
 */

/**
 * @brief Sends a message to @p whom.
 *
 * <b>Usage example:</b>
 * @code
 * self << make_any_tuple(1, 2, 3);
 * @endcode
 * @param whom Receiver of the message.
 * @param what Message as instance of {@link any_tuple}.
 * @returns @p whom.
 */
template<class C>
inline typename enable_if_channel<C, const intrusive_ptr<C>&>::type
operator<<(const intrusive_ptr<C>& whom, any_tuple what) {
    send_tuple(whom, std::move(what));
    return whom;
}

inline const self_type& operator<<(const self_type& s, any_tuple what) {
    send_tuple(s.get(), std::move(what));
    return s;
}

/**
 * @}
 */

inline actor_ptr eval_sopts(spawn_options opts, local_actor_ptr ptr) {
    CPPA_LOGF_INFO("spawned new local actor with ID " << ptr->id()
                   << " of type " << detail::demangle(typeid(*ptr)));
    if (has_monitor_flag(opts)) self->monitor(ptr);
    if (has_link_flag(opts)) self->link_to(ptr);
    return std::move(ptr);
}

/**
 * @ingroup ActorCreation
 * @{
 */

/**
 * @brief Spawns a new {@link actor} that evaluates given arguments.
 * @param args A functor followed by its arguments.
 * @tparam Options Optional flags to modify <tt>spawn</tt>'s behavior.
 * @returns An {@link actor_ptr} to the spawned {@link actor}.
 */
template<spawn_options Options = no_spawn_options, typename... Ts>
actor_ptr spawn(Ts&&... args) {
    static_assert(sizeof...(Ts) > 0, "too few arguments provided");
    return eval_sopts(Options,
                      get_scheduler()->exec(Options,
                                            scheduler::init_callback{},
                                            std::forward<Ts>(args)...));
}

/**
 * @brief Spawns an actor of type @p Impl.
 * @param args Constructor arguments.
 * @tparam Impl Subtype of {@link event_based_actor} or {@link sb_actor}.
 * @tparam Options Optional flags to modify <tt>spawn</tt>'s behavior.
 * @returns An {@link actor_ptr} to the spawned {@link actor}.
 */
template<class Impl, spawn_options Options = no_spawn_options, typename... Ts>
actor_ptr spawn(Ts&&... args) {
    static_assert(std::is_base_of<event_based_actor, Impl>::value,
                  "Impl is not a derived type of event_based_actor");
    scheduled_actor_ptr ptr;
    if (has_priority_aware_flag(Options)) {
        using derived = typename extend<Impl>::template with<threaded, prioritizing>;
        ptr = make_counted<derived>(std::forward<Ts>(args)...);
    }
    else if (has_detach_flag(Options)) {
        using derived = typename extend<Impl>::template with<threaded>;
        ptr = make_counted<derived>(std::forward<Ts>(args)...);
    }
    else ptr = make_counted<Impl>(std::forward<Ts>(args)...);
    return eval_sopts(Options, get_scheduler()->exec(Options, std::move(ptr)));
}

/**
 * @brief Spawns a new actor that evaluates given arguments and
 *        immediately joins @p grp.
 * @param args A functor followed by its arguments.
 * @tparam Options Optional flags to modify <tt>spawn</tt>'s behavior.
 * @returns An {@link actor_ptr} to the spawned {@link actor}.
 * @note The spawned has joined the group before this function returns.
 */
template<spawn_options Options = no_spawn_options, typename... Ts>
actor_ptr spawn_in_group(const group_ptr& grp, Ts&&... args) {
    static_assert(sizeof...(Ts) > 0, "too few arguments provided");
    auto init_cb = [=](local_actor* ptr) {
        ptr->join(grp);
    };
    return eval_sopts(Options,
                      get_scheduler()->exec(Options,
                                            init_cb,
                                            std::forward<Ts>(args)...));
}

/**
 * @brief Spawns an actor of type @p Impl that immediately joins @p grp.
 * @param args Constructor arguments.
 * @tparam Impl Subtype of {@link event_based_actor} or {@link sb_actor}.
 * @tparam Options Optional flags to modify <tt>spawn</tt>'s behavior.
 * @returns An {@link actor_ptr} to the spawned {@link actor}.
 * @note The spawned has joined the group before this function returns.
 */
template<class Impl, spawn_options Options = no_spawn_options, typename... Ts>
actor_ptr spawn_in_group(const group_ptr& grp, Ts&&... args) {
    auto ptr = make_counted<Impl>(std::forward<Ts>(args)...);
    ptr->join(grp);
    return eval_sopts(Options, get_scheduler()->exec(Options, ptr));
}

/** @} */

/**
 * @brief Blocks execution of this actor until all
 *        other actors finished execution.
 * @warning This function will cause a deadlock if called from multiple actors.
 * @warning Do not call this function in cooperatively scheduled actors.
 */
inline void await_all_others_done() {
    auto value = (self.unchecked() == nullptr) ? 0 : 1;
    get_actor_registry()->await_running_count_equal(value);
}

/**
 * @brief Publishes @p whom at @p port.
 *
 * The connection is automatically closed if the lifetime of @p whom ends.
 * @param whom Actor that should be published at @p port.
 * @param port Unused TCP port.
 * @param addr The IP address to listen to, or @p INADDR_ANY if @p addr is
 *             @p nullptr.
 * @throws bind_failure
 */
void publish(actor_ptr whom, std::uint16_t port, const char* addr = nullptr);

/**
 * @brief Publishes @p whom using @p acceptor to handle incoming connections.
 *
 * The connection is automatically closed if the lifetime of @p whom ends.
 * @param whom Actor that should be published at @p port.
 * @param acceptor Network technology-specific acceptor implementation.
 */
void publish(actor_ptr whom, std::unique_ptr<io::acceptor> acceptor);

/**
 * @brief Establish a new connection to the actor at @p host on given @p port.
 * @param host Valid hostname or IP address.
 * @param port TCP port.
 * @returns An {@link actor_ptr} to the proxy instance
 *          representing a remote actor.
 */
actor_ptr remote_actor(const char* host, std::uint16_t port);

/**
 * @copydoc remote_actor(const char*, std::uint16_t)
 */
inline actor_ptr remote_actor(const std::string& host, std::uint16_t port) {
    return remote_actor(host.c_str(), port);
}

/**
 * @brief Establish a new connection to the actor via given @p connection.
 * @param connection A connection to another libcppa process described by a pair
 *                   of input and output stream.
 * @returns An {@link actor_ptr} to the proxy instance
 *          representing a remote actor.
 */
actor_ptr remote_actor(io::stream_ptr_pair connection);

/**
 * @brief Spawns an IO actor of type @p Impl.
 * @param args Constructor arguments.
 * @tparam Impl Subtype of {@link io::broker}.
 * @tparam Options Optional flags to modify <tt>spawn</tt>'s behavior.
 * @returns An {@link actor_ptr} to the spawned {@link actor}.
 */
template<class Impl, spawn_options Options = no_spawn_options, typename... Ts>
actor_ptr spawn_io(io::input_stream_ptr in,
                   io::output_stream_ptr out,
                   Ts&&... args) {
    using namespace io;
    auto mm = get_middleman();
    auto ptr = make_counted<Impl>(std::forward<Ts>(args)...);
    auto backend = make_counted<broker_backend>(std::move(in), std::move(out), ptr);
    backend->init();
    mm->run_later([=] { mm->continue_reader(backend); });
    return eval_sopts(Options, std::move(ptr));
}

/**
 * @brief Spawns a new {@link actor} that evaluates given arguments.
 * @param args A functor followed by its arguments.
 * @tparam Options Optional flags to modify <tt>spawn</tt>'s behavior.
 * @returns An {@link actor_ptr} to the spawned {@link actor}.
 */
template<spawn_options Options = no_spawn_options,
         typename F = std::function<void (io::io_handle*)>,
         typename... Ts>
actor_ptr spawn_io(F fun,
                   io::input_stream_ptr in,
                   io::output_stream_ptr out,
                   Ts&&... args) {
    using namespace io;
    auto mm = get_middleman();
    auto ptr = broker::from(std::move(fun), std::forward<Ts>(args)...);
    auto backend = make_counted<broker_backend>(std::move(in), std::move(out), ptr);
    backend->init();
    mm->run_later([=] { mm->continue_reader(backend); });
    return eval_sopts(Options, std::move(ptr));
}

/**
 * @brief Destroys all singletons, disconnects all peers and stops the
 *        scheduler. It is recommended to use this function as very last
 *        function call before leaving main(). Especially in programs
 *        using libcppa's networking infrastructure.
 */
void shutdown(); // note: implemented in singleton_manager.cpp

/**
 * @brief Sends an exit message to @p whom with @p reason.
 *
 * This function is syntactic sugar for
 * <tt>send(whom, atom("EXIT"), reason)</tt>.
 * @pre <tt>reason != exit_reason::normal</tt>
 */
inline void send_exit(actor_ptr whom, std::uint32_t reason) {
    CPPA_REQUIRE(reason != exit_reason::normal);
    send(std::move(whom), atom("EXIT"), reason);
}

/**
 * @brief Sets the actor's behavior and discards the previous behavior
 *        unless {@link keep_behavior} is given as first argument.
 */
template<typename T, typename... Ts>
inline typename std::enable_if<
    !is_behavior_policy<typename util::rm_const_and_ref<T>::type>::value,
    void
>::type
become(T&& arg, Ts&&... args) {
    self->do_become(match_expr_convert(std::forward<T>(arg),
                                       std::forward<Ts>(args)...),
                    true);
}

template<bool Discard, typename... Ts>
inline void become(behavior_policy<Discard>, Ts&&... args) {
    self->do_become(match_expr_convert(std::forward<Ts>(args)...), Discard);
}

/**
 * @brief Returns to a previous behavior if available.
 */
inline void unbecome() {
    self->do_unbecome();
}

struct actor_ostream {

    typedef const actor_ostream& (*fun_type)(const actor_ostream&);

    constexpr actor_ostream() { }

    inline const actor_ostream& write(std::string arg) const {
        send(get_scheduler()->printer(), atom("add"), move(arg));
        return *this;
    }

    inline const actor_ostream& flush() const {
        send(get_scheduler()->printer(), atom("flush"));
        return *this;
    }

};

namespace { constexpr actor_ostream aout; }

inline const actor_ostream& operator<<(const actor_ostream& o, std::string arg) {
    return o.write(move(arg));
}

inline const actor_ostream& operator<<(const actor_ostream& o, const any_tuple& arg) {
    return o.write(cppa::to_string(arg));
}

template<typename T>
inline typename std::enable_if<
       !std::is_convertible<T, std::string>::value
    && !std::is_convertible<T, any_tuple>::value,
    const actor_ostream&
>::type
operator<<(const actor_ostream& o, T&& arg) {
    return o.write(std::to_string(std::forward<T>(arg)));
}

inline const actor_ostream& operator<<(const actor_ostream& o, actor_ostream::fun_type f) {
    return f(o);
}

} // namespace cppa

namespace std {
// allow actor_ptr to be used in hash maps
template<>
struct hash<cppa::actor_ptr> {
    inline size_t operator()(const cppa::actor_ptr& ptr) const {
        return (ptr) ? static_cast<size_t>(ptr->id()) : 0;
    }
};
// provide convenience overlaods for aout; implemented in logging.cpp
const cppa::actor_ostream& endl(const cppa::actor_ostream& o);
const cppa::actor_ostream& flush(const cppa::actor_ostream& o);
} // namespace std

#endif // CPPA_HPP
