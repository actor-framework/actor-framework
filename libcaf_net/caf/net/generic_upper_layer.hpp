// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"

namespace caf::net {

/// Bundles protocol-independent, generic member functions for (almost all)
/// upper layers.
class CAF_NET_EXPORT generic_upper_layer {
public:
  virtual ~generic_upper_layer();

  /// Prepares any pending data for sending.
  /// @returns `false` in case of an error to cause the lower layer to stop,
  ///          `true` otherwise.
  [[nodiscard]] virtual bool prepare_send() = 0;

  /// Queries whether all pending data has been sent. The lower calls this
  /// function to decide whether it has to wait for write events on the socket.
  [[nodiscard]] virtual bool done_sending() = 0;

  /// Called by the lower layer after some event triggered re-registering the
  /// socket manager for read operations after it has been stopped previously
  /// by the read policy. May restart consumption of bytes or messages.
  virtual void continue_reading() = 0;

  /// Called by the lower layer for cleaning up any state in case of an error.
  virtual void abort(const error& reason) = 0;
};

} // namespace caf::net
