// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/generic_lower_layer.hpp"
#include "caf/net/generic_upper_layer.hpp"

namespace caf::net::stream_oriented {

class lower_layer;

/// The upper layer requests bytes from the lower layer and consumes raw chunks
/// of data.
class CAF_NET_EXPORT upper_layer : public generic_upper_layer {
public:
  virtual ~upper_layer();

  /// Initializes the upper layer.
  /// @param down A pointer to the lower layer that remains valid for the
  ///             lifetime of the upper layer.
  virtual error start(lower_layer* down, const settings& config) = 0;

  /// Consumes bytes from the lower layer.
  /// @param buffer Available bytes to read.
  /// @param delta Bytes that arrived since last calling this function.
  /// @returns The number of consumed bytes. May be zero if waiting for more
  ///          input or negative to signal an error.
  [[nodiscard]] virtual ptrdiff_t consume(byte_span buffer, byte_span delta)
    = 0;
};

/// Provides access to a resource that operates on a byte stream, e.g., a TCP
/// socket.
class CAF_NET_EXPORT lower_layer : public generic_lower_layer {
public:
  virtual ~lower_layer();

  /// Queries whether the transport is currently configured to read from its
  /// socket.
  virtual bool is_reading() const noexcept = 0;

  /// Configures threshold for the next receive operations. Policies remain
  /// active until calling this function again.
  /// @warning Calling this function in `consume` invalidates both byte spans.
  virtual void configure_read(receive_policy policy) = 0;

  /// Prepares the layer for outgoing traffic, e.g., by allocating an output
  /// buffer as necessary.
  virtual void begin_output() = 0;

  /// Returns a reference to the output buffer. Users may only call this
  /// function and write to the buffer between calling `begin_output()` and
  /// `end_output()`.
  virtual byte_buffer& output_buffer() = 0;

  /// Prepares written data for transfer, e.g., by flushing buffers or
  /// registering sockets for write events.
  virtual bool end_output() = 0;
};

} // namespace caf::net::stream_oriented
