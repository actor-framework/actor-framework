// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/error.hpp"
#include "caf/fwd.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/http/fwd.hpp"

namespace caf::net::http {

/// Operates on HTTP requests.
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

  /// Called by the lower layer for preparing outgoing data.
  virtual bool prepare_send() = 0;

  /// Queries whether any pending sends have been completed.
  virtual bool done_sending() = 0;

  /// Consumes an HTTP message.
  /// @param ctx Identifies this request. The response message must pass this
  ///            back to the lower layer. Allows clients to send multiple
  ///            requests in "parallel" (i.e., send multiple requests before
  ///            receiving the response on the first one).
  /// @param hdr The header fields for the received message.
  /// @param payload The payload of the received message.
  /// @returns The number of consumed bytes or a negative value to signal an
  ///          error.
  /// @note Discarded data is lost permanently.
  virtual ptrdiff_t
  consume(context ctx, const header& hdr, const_byte_span payload)
    = 0;

  /// Called by the lower layer after some event triggered re-registering the
  /// socket manager for read operations after it has been stopped previously
  /// by the read policy. May restart consumption of bytes by setting a new
  /// read policy.
  virtual void continue_reading() = 0;
};

} // namespace caf::net::http
