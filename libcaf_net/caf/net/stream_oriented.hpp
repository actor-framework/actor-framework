// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/fwd.hpp"
#include "caf/net/fwd.hpp"

namespace caf::net {

/// A contract between two stream-oriented layers. The lower layer provides
/// access to a resource that operates on a byte stream, e.g., a TCP socket. The
/// upper layer requests bytes and consumes raw chunks of data.
class stream_oriented {
public:
  class lower_layer;

  class upper_layer {
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

    /// Called by the lower layer for cleaning up any state in case of an error.
    virtual void abort(const error& reason) = 0;

    /// Consumes bytes from the lower layer.
    /// @param buffer Available bytes to read.
    /// @param delta Bytes that arrived since last calling this function.
    /// @returns The number of consumed bytes. May be zero if waiting for more
    ///          input or negative to signal an error.
    virtual ptrdiff_t consume(byte_span buffer, byte_span delta) = 0;

    /// Called by the lower layer after some event triggered re-registering the
    /// socket manager for read operations after it has been stopped previously
    /// by the read policy. May restart consumption of bytes by setting a new
    /// read policy.
    virtual void continue_reading() = 0;

    /// Prepares any pending data for sending.
    virtual bool prepare_send() = 0;

    /// Queries whether all pending data has been sent.
    virtual bool done_sending() = 0;
  };

  class lower_layer {
  public:
    virtual ~lower_layer();

    /// Queries whether the output device can accept more data straight away.
    virtual bool can_send_more() const noexcept = 0;

    /// Queries whether the current read policy would cause the transport layer
    /// to stop receiving additional bytes.
    virtual bool stopped() const noexcept = 0;

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
};

} // namespace caf::net
