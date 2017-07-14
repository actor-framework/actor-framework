/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_ALL_HPP
#define CAF_ALL_HPP

#include "caf/config.hpp"

#include "caf/sec.hpp"
#include "caf/atom.hpp"
#include "caf/send.hpp"
#include "caf/skip.hpp"
#include "caf/unit.hpp"
#include "caf/term.hpp"
#include "caf/actor.hpp"
#include "caf/after.hpp"
#include "caf/error.hpp"
#include "caf/group.hpp"
#include "caf/extend.hpp"
#include "caf/logger.hpp"
#include "caf/others.hpp"
#include "caf/result.hpp"
#include "caf/stream.hpp"
#include "caf/message.hpp"
#include "caf/node_id.hpp"
#include "caf/behavior.hpp"
#include "caf/duration.hpp"
#include "caf/expected.hpp"
#include "caf/exec_main.hpp"
#include "caf/resumable.hpp"
#include "caf/streambuf.hpp"
#include "caf/to_string.hpp"
#include "caf/actor_addr.hpp"
#include "caf/actor_pool.hpp"
#include "caf/attachable.hpp"
#include "caf/message_id.hpp"
#include "caf/replies_to.hpp"
#include "caf/serializer.hpp"
#include "caf/actor_proxy.hpp"
#include "caf/exit_reason.hpp"
#include "caf/local_actor.hpp"
#include "caf/ref_counted.hpp"
#include "caf/typed_actor.hpp"
#include "caf/actor_system.hpp"
#include "caf/deserializer.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/actor_ostream.hpp"
#include "caf/function_view.hpp"
#include "caf/index_mapping.hpp"
#include "caf/spawn_options.hpp"
#include "caf/abstract_actor.hpp"
#include "caf/abstract_group.hpp"
#include "caf/blocking_actor.hpp"
#include "caf/deep_to_string.hpp"
#include "caf/execution_unit.hpp"
#include "caf/memory_managed.hpp"
#include "caf/stateful_actor.hpp"
#include "caf/typed_behavior.hpp"
#include "caf/proxy_registry.hpp"
#include "caf/behavior_policy.hpp"
#include "caf/message_builder.hpp"
#include "caf/message_handler.hpp"
#include "caf/response_handle.hpp"
#include "caf/system_messages.hpp"
#include "caf/abstract_channel.hpp"
#include "caf/may_have_timeout.hpp"
#include "caf/message_priority.hpp"
#include "caf/typed_actor_view.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/composed_behavior.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/primitive_variant.hpp"
#include "caf/timeout_definition.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/composable_behavior.hpp"
#include "caf/typed_actor_pointer.hpp"
#include "caf/scoped_execution_unit.hpp"
#include "caf/typed_response_promise.hpp"
#include "caf/typed_event_based_actor.hpp"
#include "caf/abstract_composable_behavior.hpp"

#include "caf/decorator/sequencer.hpp"

#include "caf/meta/type_name.hpp"
#include "caf/meta/annotation.hpp"
#include "caf/meta/save_callback.hpp"
#include "caf/meta/load_callback.hpp"
#include "caf/meta/omittable_if_empty.hpp"

#include "caf/scheduler/test_coordinator.hpp"
#include "caf/scheduler/abstract_coordinator.hpp"

/// @author Dominik Charousset <dominik.charousset (at) haw-hamburg.de>
///
/// @mainpage CAF
///
/// @section Intro Introduction
///
/// This library provides an implementation of the actor model for C++.
/// It uses a network transparent messaging system to ease development
/// of both concurrent and distributed software.
///
/// `libcaf` uses a thread pool to schedule actors by default.
/// A scheduled actor should not call blocking functions.
/// Individual actors can be spawned (created) with a special flag to run in
/// an own thread if one needs to make use of blocking APIs.
///
/// Writing applications in `libcaf` requires a minimum of gluecode and
/// each context <i>is</i> an actor. Even main is implicitly
/// converted to an actor if needed.
///
/// @section GettingStarted Getting Started
///
/// To build `libcaf,` you need `GCC >= 4.8 or <tt>Clang >=
///3.2</tt>,
/// and `CMake`.
///
/// The usual build steps on Linux and Mac OS X are:
///
///- `mkdir build
///- `cd build
///- `cmake ..
///- `make
///- `make install (as root, optionally)
///
/// Please run the unit tests as well to verify that `libcaf`
/// works properly.
///
///- `./bin/unit_tests
///
/// Please submit a bug report that includes (a) your compiler version,
/// (b) your OS, and (c) the output of the unit tests if an error occurs.
///
/// Windows is not supported yet, because MVSC++ doesn't implement the
/// C++11 features needed to compile `libcaf`.
///
/// Please read the <b>Manual</b> for an introduction to `libcaf`.
/// It is available online as HTML at
/// http://neverlord.github.com/libcaf/manual/index.html or as PDF at
/// http://neverlord.github.com/libcaf/manual/manual.pdf
///
/// @section IntroHelloWorld Hello World Example
///
/// @include hello_world.cpp
///
/// @section IntroMoreExamples More Examples
///
/// The {@link math_actor.cpp Math Actor Example} shows the usage
/// of {@link receive_loop} and {@link caf::arg_match arg_match}.
/// The {@link dining_philosophers.cpp Dining Philosophers Example}
/// introduces event-based actors covers various features of CAF.
///
/// @namespace caf
/// Root namespace of libcaf.
///
/// @namespace caf::mixin
/// Contains mixin classes implementing several actor traits.
///
/// @namespace caf::exit_reason
/// Contains all predefined exit reasons.
///
/// @namespace caf::policy
/// Contains policies encapsulating characteristics or algorithms.
///
/// @namespace caf::io
/// Contains all IO-related classes and functions.
///
/// @namespace caf::io::network
/// Contains classes and functions used for network abstraction.
///
/// @namespace caf::io::basp
/// Contains all classes and functions for the Binary Actor Sytem Protocol.
///
/// @defgroup MessageHandling Message Handling
///
/// This is the beating heart of CAF, since actor programming is
/// a message oriented programming paradigm.
///
/// A message in CAF is a n-tuple of values (with size >= 1).
/// You can use almost every type in a messages as long as it is announced,
/// i.e., known by the type system of CAF.
///
/// @defgroup BlockingAPI Blocking API
///
/// Blocking functions to receive messages.
///
/// The blocking API of CAF is intended to be used for migrating
/// previously threaded applications. When writing new code, you should
/// consider the nonblocking API based on `become` and `unbecome` first.
///
/// @section Send Sending Messages
///
/// The function `send` can be used to send a message to an actor.
/// The first argument is the receiver of the message followed by any number
/// of values:
///
/// ~~
/// // spawn some actors
/// auto a1 = spawn(...);
/// auto a2 = spawn(...);
/// auto a3 = spawn(...);
///
/// // send a message to a1
/// send(a1, atom("hello"), "hello a1!");
///
/// // send a message to a1, a2, and a3
/// auto msg = make_message(atom("compute"), 1, 2, 3);
/// send(a1, msg);
/// send(a2, msg);
/// send(a3, msg);
/// ~~
///
/// @section Receive Receive messages
///
/// The function `receive` takes a `behavior` as argument. The behavior
/// is a list of { pattern >> callback } rules.
///
/// ~~
/// receive
/// (
///   on(atom("hello"), arg_match) >> [](const std::string& msg)
///   {
///     cout << "received hello message: " << msg << endl;
///   },
///   on(atom("compute"), arg_match) >> [](int i0, int i1, int i2)
///   {
///     // send our result back to the sender of this messages
///     return make_message(atom("result"), i0 + i1 + i2);
///   }
/// );
/// ~~
///
/// Please read the manual for further details about pattern matching.
///
/// @section Atoms Atoms
///
/// Atoms are a nice way to add semantic informations to a message.
/// Assuming an actor wants to provide a "math sevice" for integers. It
/// could provide operations such as addition, subtraction, etc.
/// This operations all have two operands. Thus, the actor does not know
/// what operation the sender of a message wanted by receiving just two integers.
///
/// Example actor:
/// ~~
/// void math_actor() {
///   receive_loop (
///     on(atom("plus"), arg_match) >> [](int a, int b) {
///       return make_message(atom("result"), a + b);
///     },
///     on(atom("minus"), arg_match) >> [](int a, int b) {
///       return make_message(atom("result"), a - b);
///     }
///   );
/// }
/// ~~
///
/// @section ReceiveLoops Receive Loops
///
/// Previous examples using `receive` create behaviors on-the-fly.
/// This is inefficient in a loop since the argument passed to receive
/// is created in each iteration again. It's possible to store the behavior
/// in a variable and pass that variable to receive. This fixes the issue
/// of re-creation each iteration but rips apart definition and usage.
///
/// There are four convenience functions implementing receive loops to
/// declare behavior where it belongs without unnecessary
/// copies: `receive_loop,` `receive_while,` `receive_for` and `do_receive`.
///
/// `receive_loop` is analogous to `receive` and loops "forever" (until the
/// actor finishes execution).
///
/// `receive_while` creates a functor evaluating a lambda expression.
/// The loop continues until the given lambda returns `false`. A simple example:
///
/// ~~
/// // receive two integers
/// vector<int> received_values;
/// receive_while([&]() { return received_values.size() < 2; }) (
///   on<int>() >> [](int value) {
///     received_values.push_back(value);
///   }
/// );
/// // ...
/// ~~
///
/// `receive_for` is a simple ranged-based loop:
///
/// ~~
/// std::vector<int> vec {1, 2, 3, 4};
/// auto i = vec.begin();
/// receive_for(i, vec.end()) (
///   on(atom("get")) >> [&]() -> message { return {atom("result"), *i}; }
/// );
/// ~~
///
/// `do_receive` returns a functor providing the function `until` that
/// takes a lambda expression. The loop continues until the given lambda
/// returns true. Example:
///
/// ~~
/// // receive ints until zero was received
/// vector<int> received_values;
/// do_receive (
///   on<int>() >> [](int value) {
///     received_values.push_back(value);
///   }
/// )
/// .until([&]() { return received_values.back() == 0 });
/// // ...
/// ~~
///
/// @section FutureSend Sending Delayed Messages
///
/// The function `delayed_send` provides a simple way to delay a message.
/// This is particularly useful for recurring events, e.g., periodical polling.
/// Usage example:
///
/// ~~
/// delayed_send(self, std::chrono::seconds(1), poll_atom::value);
/// receive_loop (
///   // ...
///   [](poll_atom) {
///     // ... poll something ...
///     // and do it again after 1sec
///     delayed_send(self, std::chrono::seconds(1), poll_atom::value);
///   }
/// );
/// ~~
///
/// See also the {@link dancing_kirby.cpp dancing kirby example}.
///
/// @defgroup ImplicitConversion Implicit Type Conversions
///
/// The message passing of `libcaf` prohibits pointers in messages because
/// it enforces network transparent messaging.
/// Unfortunately, string literals in `C++` have the type `const char*,
/// resp. `const char[]. Since `libcaf` is a user-friendly library,
/// it silently converts string literals and C-strings to `std::string` objects.
/// It also converts unicode literals to the corresponding STL container.
///
/// A few examples:
/// ~~
/// // sends an std::string containing "hello actor!" to itself
/// send(self, "hello actor!");
///
/// const char* cstring = "cstring";
/// // sends an std::string containing "cstring" to itself
/// send(self, cstring);
///
/// // sends an std::u16string containing the UTF16 string "hello unicode world!"
/// send(self, u"hello unicode world!");
///
/// // x has the type caf::tuple<std::string, std::string>
/// auto x = make_message("hello", "tuple");
///
/// receive (
///   // equal to: on(std::string("hello actor!"))
///   on("hello actor!") >> [] { }
/// );
/// ~~
///
/// @defgroup ActorCreation Creating Actors

// examples

/// A trivial example program.
/// @example hello_world.cpp

/// A simple example for a delayed_send based application.
/// @example dancing_kirby.cpp

/// An event-based "Dining Philosophers" implementation.
/// @example dining_philosophers.cpp

#endif // CAF_ALL_HPP
