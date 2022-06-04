// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/net_export.hpp"
#include "caf/error.hpp"
#include "caf/fwd.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/generic_lower_layer.hpp"
#include "caf/net/generic_upper_layer.hpp"

namespace caf::net::message_oriented {

class lower_layer;

/// Consumes binary messages from the lower layer.
class CAF_NET_EXPORT upper_layer : public generic_upper_layer {
public:
  virtual ~upper_layer();

  /// Initializes the upper layer.
  /// @param owner A pointer to the socket manager that owns the entire
  ///              protocol stack. Remains valid for the lifetime of the upper
  ///              layer.
  /// @param down A pointer to the lower layer that remains valid for the
  ///             lifetime of the upper layer.
  virtual error
  init(socket_manager* owner, lower_layer* down, const settings& config)
    = 0;

  /// Consumes bytes from the lower layer.
  /// @param payload Payload of the received message.
  /// @returns The number of consumed bytes or a negative value to signal an
  ///          error.
  /// @note Discarded data is lost permanently.
  [[nodiscard]] virtual ptrdiff_t consume(byte_span payload) = 0;
};

/// Provides access to a resource that operates on the granularity of messages,
/// e.g., a UDP socket.
class CAF_NET_EXPORT lower_layer : public generic_lower_layer {
public:
  virtual ~lower_layer();

  /// Pulls messages from the transport until calling `suspend_reading`.
  virtual void request_messages() = 0;

  /// Prepares the layer for an outgoing message, e.g., by allocating an
  /// output buffer as necessary.
  virtual void begin_message() = 0;

  /// Returns a reference to the buffer for assembling the current message.
  /// Users may only call this function and write to the buffer between
  /// calling `begin_message()` and `end_message()`.
  /// @note The lower layers may pre-fill the buffer, e.g., to prefix custom
  ///       headers.
  virtual byte_buffer& message_buffer() = 0;

  /// Seals and prepares a message for transfer.
  /// @note When returning `false`, clients must also call
  ///       `down.set_read_error(...)` with an appropriate error code.
  virtual bool end_message() = 0;

  /// Inform the remote endpoint that no more messages will arrive.
  /// @note Not every protocol has a dedicated close message. Some
  ///       implementation may simply do nothing.
  virtual void send_close_message() = 0;

  /// Inform the remote endpoint that no more messages will arrive because of
  /// an error.
  /// @note Not every protocol has a dedicated close message. Some
  ///       implementation may simply do nothing.
  virtual void send_close_message(const error& reason) = 0;
};

} // namespace caf::net::message_oriented
