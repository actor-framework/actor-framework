// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/abstract_actor.hpp"
#include "caf/actor.hpp"
#include "caf/actor_addr.hpp"
#include "caf/actor_clock.hpp"
#include "caf/actor_from_state.hpp"
#include "caf/actor_pool.hpp"
#include "caf/actor_proxy.hpp"
#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/actor_traits.hpp"
#include "caf/after.hpp"
#include "caf/anon_mail.hpp"
#include "caf/attachable.hpp"
#include "caf/behavior.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/blocking_actor.hpp"
#include "caf/byte_buffer.hpp"
#include "caf/byte_span.hpp"
#include "caf/caf_main.hpp"
#include "caf/config.hpp"
#include "caf/config_option.hpp"
#include "caf/config_option_adder.hpp"
#include "caf/config_value.hpp"
#include "caf/config_value_reader.hpp"
#include "caf/config_value_writer.hpp"
#include "caf/const_typed_message_view.hpp"
#include "caf/deep_to_string.hpp"
#include "caf/defaults.hpp"
#include "caf/deserializer.hpp"
#include "caf/error.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/exec_main.hpp"
#include "caf/exit_reason.hpp"
#include "caf/expected.hpp"
#include "caf/extend.hpp"
#include "caf/function_view.hpp"
#include "caf/hash/fnv.hpp"
#include "caf/init_global_meta_objects.hpp"
#include "caf/keep_behavior.hpp"
#include "caf/local_actor.hpp"
#include "caf/logger.hpp"
#include "caf/make_config_option.hpp"
#include "caf/may_have_timeout.hpp"
#include "caf/message.hpp"
#include "caf/message_builder.hpp"
#include "caf/message_handler.hpp"
#include "caf/message_id.hpp"
#include "caf/message_priority.hpp"
#include "caf/mtl.hpp"
#include "caf/node_id.hpp"
#include "caf/others.hpp"
#include "caf/proxy_registry.hpp"
#include "caf/raise_error.hpp"
#include "caf/ref_counted.hpp"
#include "caf/result.hpp"
#include "caf/resumable.hpp"
#include "caf/scheduler.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/sec.hpp"
#include "caf/serializer.hpp"
#include "caf/skip.hpp"
#include "caf/spawn_options.hpp"
#include "caf/stateful_actor.hpp"
#include "caf/system_messages.hpp"
#include "caf/term.hpp"
#include "caf/thread_hook.hpp"
#include "caf/timeout_definition.hpp"
#include "caf/type_id.hpp"
#include "caf/typed_actor.hpp"
#include "caf/typed_actor_pointer.hpp"
#include "caf/typed_actor_view.hpp"
#include "caf/typed_behavior.hpp"
#include "caf/typed_event_based_actor.hpp"
#include "caf/typed_message_view.hpp"
#include "caf/typed_response_promise.hpp"
#include "caf/unit.hpp"
#include "caf/uuid.hpp"

/// @mainpage CAF
///
/// @section Intro Introduction
///
/// This framework provides an implementation of the actor model for C++. It
/// uses network transparent messaging to ease development of concurrent and
/// distributed software.
///
/// To get started with the framework, please read the
/// [manual](https://actor-framework.readthedocs.io) first.
///
/// @section IntroHelloWorld Hello World Example
///
/// @include hello_world.cpp
///
/// @section IntroMoreExamples More Examples
///
/// The {@link dining_philosophers.cpp Dining Philosophers Example}
/// introduces event-based actors covers various features of CAF.
///
/// @namespace caf
/// Root namespace of libcaf.
///
/// @namespace caf::mixin
/// Contains mixin classes implementing several actor traits.
///
/// @namespace caf::policy
/// Contains policies encapsulating characteristics or algorithms.
///
/// @namespace caf::telemetry
/// Contains classes and functions for collecting telemetry data.
///
/// @namespace caf::net
/// Contains all classes and functions related to network protocols.
///
/// @namespace caf::net::dsl
/// Contains building blocks to assemble protocol stacks in a declarative way.
///
/// @namespace caf::net::http
/// Contains an implementation for HTTP.
///
/// @namespace caf::net::ssl
/// Contains wrappers for convenient access to SSL.
///
/// @namespace caf::net::octet_stream
/// Contains classes and utilities for transports that operate on raw octets.
///
/// @namespace caf::net::prometheus
/// Contains a scraper for exposing metrics from an actor system to Prometheus.
///
/// @namespace caf::net::web_socket
/// Contains an implementation for message exchange over the WebSocket protocol.
///
/// @namespace caf::net::lp
/// Contains an implementation for message exchange over length-prefix framing.
///
/// @namespace caf::io
/// Contains all IO-related classes and functions.
///
/// @namespace caf::io::network
/// Contains classes and functions used for network abstraction.
///
/// @namespace caf::io::basp
/// Contains all classes and functions for the Binary Actor System Protocol.
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
/// @code
/// // spawn some actors
/// actor_system_config cfg;
/// actor_system system{cfg};
/// auto a1 = system.spawn(...);
/// auto a2 = system.spawn(...);
/// auto a3 = system.spawn(...);
///
/// // an actor executed in the current thread
/// scoped_actor self{system};
///
/// // define an atom for message annotation
/// using hello_atom = atom_constant<atom("hello")>;
/// using compute_atom = atom_constant<atom("compute")>;
/// using result_atom = atom_constant<atom("result")>;
///
/// // send a message to a1
/// self->mail(hello_atom::value, "hello a1!").send(a1);
///
/// // send a message to a1, a2, and a3
/// auto msg = make_message(compute_atom::value, 1, 2, 3);
/// self->mail(msg).send(a1);
/// self->mail(msg).send(a2);
/// self->mail(msg).send(a3);
/// @endcode
///
/// @section Receive Receive messages
///
/// The function `receive` takes a `behavior` as argument. The behavior
/// is a list of { callback } rules where the callback argument types
/// define a pattern for matching messages.
///
/// @code
/// {
///   [&self](hello_atom, const std::string& msg) {
///     self->println("received hello message: {}", msg);
///   },
///   [](compute_atom, int i0, int i1, int i2) {
///     // send our result back to the sender of this messages
///     return make_message(result_atom::value, i0 + i1 + i2);
///   }
/// }
/// @endcode
///
/// Blocking actors such as the scoped actor can call their receive member
/// to handle incoming messages.
///
/// @code
/// self->receive(
///  [&self](result_atom, int i) {
///    self->println("result is: {}", i);
///  }
/// );
/// @endcode
///
/// Please read the manual for further details about pattern matching.
///
/// @section Atoms Atoms
///
/// Atoms are a nice way to add semantic information to a message. Assuming an
/// actor wants to provide a "math service" for integers. It could provide
/// operations such as addition, subtraction, etc. This operations all have two
/// operands. Thus, the actor does not know what operation the sender of a
/// message wanted by receiving just two integers.
///
/// Example actor:
/// @code
/// using plus_atom = atom_constant<atom("plus")>;
/// using minus_atom = atom_constant<atom("minus")>;
/// behavior math_actor() {
///   return {
///     [](plus_atom, int a, int b) {
///       return make_message(atom("result"), a + b);
///     },
///     [](minus_atom, int a, int b) {
///       return make_message(atom("result"), a - b);
///     }
///   };
/// }
/// @endcode
///
/// @section ReceiveLoops Receive Loops
///
/// The previous examples used `receive` to create a behavior on-the-fly.
/// This is inefficient in a loop since the argument passed to receive
/// is created in each iteration again. It's possible to store the behavior
/// in a variable and pass that variable to receive. This fixes the issue
/// of re-creation each iteration but rips apart definition and usage.
///
/// There are three convenience functions implementing receive loops to
/// declare behavior where it belongs without unnecessary
/// copies: `receive_while,` `receive_for` and `do_receive`.
///
/// `receive_while` creates a functor evaluating a lambda expression.
/// The loop continues until the given lambda returns `false`. A simple example:
///
/// @code
/// size_t received = 0;
/// receive_while([&] { return received < 10; }) (
///   [&](int) {
///     ++received;
///   }
/// );
/// // ...
/// @endcode
///
/// `receive_for` is a simple ranged-based loop:
///
/// @code
/// std::vector<int> results;
/// size_t i = 0;
/// receive_for(i, 10) (
///   [&](int value) {
///     results.push_back(value);
///   }
/// );
/// @endcode
///
/// `do_receive` returns a functor providing the function `until` that
/// takes a lambda expression. The loop continues until the given lambda
/// returns true. Example:
///
/// @code
/// size_t received = 0;
/// do_receive (
///   [&](int) {
///     ++received;
///   }
/// ).until([&] { return received >= 10; });
/// // ...
/// @endcode
///
/// @section FutureSend Sending Delayed Messages
///
/// The function `delayed_send` provides a simple way to delay a message.
/// This is particularly useful for recurring events, e.g., periodical polling.
/// Usage example:
///
/// @code
/// scoped_actor self{...};
///
/// self->mail(poll_atom::value).delay(std::chrono::seconds(1)).send(self);
/// bool running = true;
/// self->receive_while([&](){ return running; }) (
///   // ...
///   [&](poll_atom) {
///     // ... poll something ...
///     // and do it again after 1sec
///     self->mail(poll_atom::value).delay(std::chrono::seconds(1)).send(self);
///   }
/// );
/// @endcode
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
/// @code
/// // sends an std::string containing "hello actor!" to itself
/// send(self, "hello actor!");
///
/// const char* cstring = "cstring";
/// // sends an std::string containing "cstring" to itself
/// send(self, cstring);
///
/// // sends an std::u16string containing "hello unicode world!"
/// send(self, u"hello unicode world!");
///
/// // x has the type caf::tuple<std::string, std::string>
/// auto x = make_message("hello", "tuple");
/// @endcode
///
/// @defgroup ActorCreation Creating Actors

// examples

/// @example hello_world.cpp
/// ### A trivial example program.

/// @example dancing_kirby.cpp
/// ### A simple example for a delayed_send based application.

/// @example dining_philosophers.cpp
/// ### An event-based "Dining Philosophers" implementation.
