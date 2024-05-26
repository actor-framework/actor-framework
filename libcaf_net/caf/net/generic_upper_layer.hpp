// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"

namespace caf::net {

/// Bundles protocol-independent, generic member functions for (almost all)
/// upper layers.
class CAF_NET_EXPORT generic_upper_layer {
public:
  virtual ~generic_upper_layer();

  /// Gives the upper layer an opportunity to add additional data to the output
  /// buffer.
  virtual void prepare_send() = 0;

  /// Queries whether all pending data has been sent. The lower calls this
  /// function to decide whether it has to wait for write events on the socket.
  [[nodiscard]] virtual bool done_sending() = 0;

  /// Called by the lower layer for cleaning up any state in case of an error or
  /// when disposed.
  virtual void abort(const error& reason) = 0;
};

} // namespace caf::net
