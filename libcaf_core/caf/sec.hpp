/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

// This file is partially included in the manual, do not modify
// without updating the references in the *.tex files!
// Manual references: lines 32-117 (Error.tex)

#pragma once

#include "caf/error.hpp"
#include "caf/make_message.hpp"

namespace caf {

/// SEC stands for "System Error Code". This enum contains error codes for
/// ::actor_system and its modules.
enum class sec : uint8_t {
  /// No error.
  none = 0,
  /// Indicates that an actor dropped an unexpected message.
  unexpected_message = 1,
  /// Indicates that a response message did not match the provided handler.
  unexpected_response,
  /// Indicates that the receiver of a request is no longer alive.
  request_receiver_down,
  /// Indicates that a request message timed out.
  request_timeout,
  /// Indicates that requested group module does not exist.
  no_such_group_module = 5,
  /// Unpublishing or connecting failed: no actor bound to given port.
  no_actor_published_at_port,
  /// Connecting failed because a remote actor had an unexpected interface.
  unexpected_actor_messaging_interface,
  /// Migration failed because the state of an actor is not serializable.
  state_not_serializable,
  /// An actor received an unsupported key for `('sys', 'get', key)` messages.
  unsupported_sys_key,
  /// An actor received an unsupported system message.
  unsupported_sys_message = 10,
  /// A remote node disconnected during CAF handshake.
  disconnect_during_handshake,
  /// Tried to forward a message via BASP to an invalid actor handle.
  cannot_forward_to_invalid_actor,
  /// Tried to forward a message via BASP to an unknown node ID.
  no_route_to_receiving_node,
  /// Middleman could not assign a connection handle to a broker.
  failed_to_assign_scribe_from_handle,
  /// Middleman could not assign an acceptor handle to a broker.
  failed_to_assign_doorman_from_handle = 15,
  /// User requested to close port 0 or to close a port not managed by CAF.
  cannot_close_invalid_port,
  /// Middleman could not connect to a remote node.
  cannot_connect_to_node,
  /// Middleman could not open requested port.
  cannot_open_port,
  /// A C system call in the middleman failed.
  network_syscall_failed,
  /// A function received one or more invalid arguments.
  invalid_argument = 20,
  /// A network socket reported an invalid network protocol family.
  invalid_protocol_family,
  /// Middleman could not publish an actor because it was invalid.
  cannot_publish_invalid_actor,
  /// A remote spawn failed because the provided types did not match.
  cannot_spawn_actor_from_arguments,
  /// Serialization failed because there was not enough data to read.
  end_of_stream,
  /// Serialization failed because no CAF context is available.
  no_context = 25,
  /// Serialization failed because CAF misses run-time type information.
  unknown_type,
  /// Serialization of actors failed because no proxy registry is available.
  no_proxy_registry,
  /// An exception was thrown during message handling.
  runtime_error,
  /// Linking to a remote actor failed because actor no longer exists.
  remote_linking_failed,
  /// Adding an upstream to a stream failed.
  cannot_add_upstream = 30,
  /// Adding an upstream to a stream failed because it already exists.
  upstream_already_exists,
  /// Unable to process upstream messages because upstream is invalid.
  invalid_upstream,
  /// Adding a downstream to a stream failed.
  cannot_add_downstream,
  /// Adding a downstream to a stream failed because it already exists.
  downstream_already_exists,
  /// Unable to process downstream messages because downstream is invalid.
  invalid_downstream = 35,
  /// Cannot start streaming without next stage.
  no_downstream_stages_defined,
  /// Actor failed to initialize state after receiving a stream handshake.
  stream_init_failed,
  /// Unable to process a stream since due to missing state.
  invalid_stream_state,
  /// Stream aborted due to unexpected error.
  unhandled_stream_error,
  /// A function view was called without assigning an actor first.
  bad_function_call = 40,
  /// Feature is disabled in the actor system config.
  feature_disabled,
};

/// @relates sec
std::string to_string(sec);

/// @relates sec
error make_error(sec);

/// @relates sec
template <class T, class... Ts>
error make_error(sec code, T&& x, Ts&&... xs) {
  return {static_cast<uint8_t>(code), atom("system"),
          make_message(std::forward<T>(x), std::forward<Ts>(xs)...)};
}

} // namespace caf

