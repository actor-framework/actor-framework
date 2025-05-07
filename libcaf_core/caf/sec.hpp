// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

// This file is partially included in the manual, do not modify
// without updating the references in the *.tex files!
// Manual references: lines 32-117 (Error.tex)

#pragma once

#include "caf/default_enum_inspect.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/is_error_code_enum.hpp"

#include <cstdint>
#include <string>
#include <type_traits>

namespace caf {

// --(rst-sec-begin)--
/// SEC stands for "System Error Code". This enum contains error codes for
/// ::actor_system and its modules.
enum class sec : uint8_t {
  /// No error.
  none = 0,
  /// Indicates that an actor dropped an unexpected message.
  unexpected_message = 1,
  /// Indicates that a response message did not match the provided handler.
  unexpected_response,
  /// Indicates that the receiver of a request is no longer alive. If an actor
  /// terminates, all pending requests to this actor are dropped from the
  /// mailbox and the sender receives an error message with this code. The error
  /// code is also used when a request is sent to an actor that has already
  /// terminated and does no longer accept messages.
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
  /// Subscribing to a stream failed because it was invalid.
  invalid_stream = 30,
  /// Subscribing to a stream failed because it can only be subscribed to once.
  cannot_resubscribe_stream,
  /// A stream was aborted by the hosting actor, usually because it terminated.
  stream_aborted,
  /// A function view was called without assigning an actor first.
  bad_function_call = 40,
  /// Feature is disabled in the actor system config.
  feature_disabled,
  /// Failed to open file.
  cannot_open_file,
  /// A socket descriptor argument is invalid.
  socket_invalid,
  /// A socket became disconnected from the remote host (hang up).
  socket_disconnected,
  /// An operation on a socket (e.g. `poll`) failed.
  socket_operation_failed = 45,
  /// A resource is temporarily unavailable or would block.
  unavailable_or_would_block,
  /// Connection refused because of incompatible CAF versions.
  incompatible_versions,
  /// Connection refused because of incompatible application IDs.
  incompatible_application_ids,
  /// Received a malformed message from another node.
  malformed_message,
  /// The middleman closed a connection because it failed to serialize or
  /// deserialize a payload.
  serializing_basp_payload_failed = 50,
  /// The middleman closed a connection to itself or an already connected node.
  redundant_connection,
  /// Resolving a path on a remote node failed.
  remote_lookup_failed,
  /// Serialization failed because actor_system::tracing_context is null.
  no_tracing_context,
  /// No request produced a valid result.
  all_requests_failed,
  /// Deserialization failed because an invariant got violated after reading
  /// the content of a field.
  field_invariant_check_failed = 55,
  /// Deserialization failed because a setter rejected the input.
  field_value_synchronization_failed,
  /// Deserialization failed because the source announced an invalid type.
  invalid_field_type,
  /// Serialization failed because a type was flagged as unsafe message type.
  unsafe_type,
  /// Serialization failed because a save callback returned `false`.
  save_callback_failed,
  /// Deserialization failed because a load callback returned `false`.
  load_callback_failed = 60,
  /// Converting between two types failed.
  conversion_failed,
  /// A network connection was closed by the remote side.
  connection_closed,
  /// An operation failed because run-time type information diverged from the
  /// expected type.
  type_clash,
  /// An operation failed because the callee does not implement this
  /// functionality.
  unsupported_operation,
  /// A key lookup failed.
  no_such_key = 65,
  /// An destroyed a response promise without calling deliver or delegate on it.
  broken_promise,
  /// Disconnected from a BASP node after reaching the connection timeout.
  connection_timeout,
  /// Signals that an actor fell behind a periodic action trigger. After raising
  /// this error, an @ref actor_clock stops scheduling the action.
  action_reschedule_failed,
  /// Attaching to an observable failed because the target is invalid.
  invalid_observable,
  /// Attaching to an observable failed because the target already reached its
  /// maximum observer count.
  too_many_observers = 70,
  /// Signals that an operation failed because the target has been disposed.
  disposed,
  /// Failed to open a resource.
  cannot_open_resource,
  /// Received malformed data.
  protocol_error,
  /// Encountered faulty logic in the program.
  logic_error,
  /// An actor tried to delegate a message to an invalid actor handle.
  invalid_delegate = 75,
  /// An actor tried to delegate a message to an invalid actor handle.
  invalid_request,
  /// Signals that `future::get` timed out.
  future_timeout,
  /// Received invalid UTF-8 encoding.
  invalid_utf8,
  /// A downstream operator failed to process inputs on time.
  backpressure_overflow,
  /// Signals that a supervisor failed to start a new worker because too many
  /// workers failed in a short period of time.
  too_many_worker_failures = 80,
  /// Signals that a flow operator failed to combine inputs from multiple
  /// input observables because at least one of them completed before emitting
  /// a single value.
  cannot_combine_empty_observables,
};
// --(rst-sec-end)--

/// @relates sec
CAF_CORE_EXPORT std::string to_string(sec);

/// @relates sec
CAF_CORE_EXPORT bool from_string(std::string_view, sec&);

/// @relates sec
CAF_CORE_EXPORT bool from_integer(std::underlying_type_t<sec>, sec&);

/// @relates sec
template <class Inspector>
bool inspect(Inspector& f, sec& x) {
  return default_enum_inspect(f, x);
}

} // namespace caf

CAF_ERROR_CODE_ENUM(sec)
