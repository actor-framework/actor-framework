// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/net_export.hpp"
#include "caf/error.hpp"
#include "caf/fwd.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/generic_lower_layer.hpp"

namespace caf::net::lp {

/// Provides access to a resource that operates on the granularity of binary
/// messages.
class CAF_NET_EXPORT lower_layer : public generic_lower_layer {
public:
  virtual ~lower_layer();

  /// Pulls messages from the transport until calling `suspend_reading`.
  virtual void request_messages() = 0;

  /// Stops reading messages until calling `request_messages`.
  virtual void suspend_reading() = 0;

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
};

} // namespace caf::net::lp
