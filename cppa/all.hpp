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
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/

#ifndef CPPA_ALL_HPP
#define CPPA_ALL_HPP

#include "cppa/on.hpp"
#include "cppa/atom.hpp"
#include "cppa/send.hpp"
#include "cppa/unit.hpp"
#include "cppa/actor.hpp"
#include "cppa/group.hpp"
#include "cppa/match.hpp"
#include "cppa/spawn.hpp"
#include "cppa/config.hpp"
#include "cppa/extend.hpp"
#include "cppa/channel.hpp"
#include "cppa/message.hpp"
#include "cppa/node_id.hpp"
#include "cppa/publish.hpp"
#include "cppa/announce.hpp"
#include "cppa/anything.hpp"
#include "cppa/behavior.hpp"
#include "cppa/duration.hpp"
#include "cppa/sb_actor.hpp"
#include "cppa/spawn_io.hpp"
#include "cppa/shutdown.hpp"
#include "cppa/exception.hpp"
#include "cppa/cow_tuple.hpp"
#include "cppa/resumable.hpp"
#include "cppa/scheduler.hpp"
#include "cppa/spawn_fwd.hpp"
#include "cppa/to_string.hpp"
#include "cppa/actor_addr.hpp"
#include "cppa/attachable.hpp"
#include "cppa/match_expr.hpp"
#include "cppa/message_id.hpp"
#include "cppa/replies_to.hpp"
#include "cppa/serializer.hpp"
#include "cppa/actor_proxy.hpp"
#include "cppa/exit_reason.hpp"
#include "cppa/from_string.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/ref_counted.hpp"
#include "cppa/typed_actor.hpp"
#include "cppa/remote_actor.hpp"
#include "cppa/deserializer.hpp"
#include "cppa/scoped_actor.hpp"
#include "cppa/skip_message.hpp"
#include "cppa/actor_ostream.hpp"
#include "cppa/spawn_options.hpp"
#include "cppa/abstract_actor.hpp"
#include "cppa/abstract_group.hpp"
#include "cppa/blocking_actor.hpp"
#include "cppa/execution_unit.hpp"
#include "cppa/memory_managed.hpp"
#include "cppa/typed_behavior.hpp"
#include "cppa/actor_namespace.hpp"
#include "cppa/behavior_policy.hpp"
#include "cppa/continue_helper.hpp"
#include "cppa/mailbox_element.hpp"
#include "cppa/message_builder.hpp"
#include "cppa/message_handler.hpp"
#include "cppa/response_handle.hpp"
#include "cppa/system_messages.hpp"
#include "cppa/abstract_channel.hpp"
#include "cppa/may_have_timeout.hpp"
#include "cppa/message_priority.hpp"
#include "cppa/response_promise.hpp"
#include "cppa/binary_serializer.hpp"
#include "cppa/event_based_actor.hpp"
#include "cppa/primitive_variant.hpp"
#include "cppa/uniform_type_info.hpp"
#include "cppa/wildcard_position.hpp"
#include "cppa/timeout_definition.hpp"
#include "cppa/options_description.hpp"
#include "cppa/binary_deserializer.hpp"
#include "cppa/publish_local_groups.hpp"
#include "cppa/await_all_actors_done.hpp"
#include "cppa/typed_continue_helper.hpp"
#include "cppa/typed_event_based_actor.hpp"

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
 * To build @p libcppa, you need <tt>GCC >= 4.7</tt> or <tt>Clang >=
 *3.2</tt>,
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
 * Please run the unit tests as well to verify that @p libcppa
 * works properly.
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
 * It is available online as HTML at
 * http://neverlord.github.com/boost.actor/manual/index.html or as PDF at
 * http://neverlord.github.com/boost.actor/manual/manual.pdf
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
 * auto x = make_message(1, 2, 3);
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
 * A message in @p libcppa is a n-tuple of values (with size >= 1)
 * You can use almost every type in a messages - as long as it is announced,
 * i.e., known by the type system of @p libcppa.
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
 * The function @p send can be used to send a message to an actor.
 * The first argument is the receiver of the message followed by any number
 * of values:
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
 * // send a message to a1, a2, and a3
 * auto msg = make_message(atom("compute"), 1, 2, 3);
 * send(a1, msg);
 * send(a2, msg);
 * send(a3, msg);
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
 *         return make_message(atom("result"), i0 + i1 + i2);
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
 *             return make_message(atom("result"), a + b);
 *         },
 *         on(atom("minus"), arg_match) >> [](int a, int b) {
 *             return make_message(atom("result"), a - b);
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
 *     on(atom("get")) >> [&]() -> message { return {atom("result"), *i}; }
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
 * auto x = make_message("hello", "tuple");
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

#endif // CPPA_ALL_HPP
