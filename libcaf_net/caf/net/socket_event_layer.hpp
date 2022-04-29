// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"
#include "caf/net/fwd.hpp"

namespace caf::net {

/// The lowest layer in a protocol stack. Called by a @ref socket_manager
/// directly.
class CAF_NET_EXPORT socket_event_layer {
public:
  virtual ~socket_event_layer();

  /// Initializes the layer.
  virtual error init(socket_manager* owner, const settings& cfg) = 0;

  /// Handles a read event on the managed socket.
  virtual void handle_read_event() = 0;

  /// Handles a write event on the managed socket.
  virtual void handle_write_event() = 0;

  /// Called after returning `handover` from a read or write handler.
  virtual bool do_handover(std::unique_ptr<socket_event_layer>& next);

  /// Called on hard errors on the managed socket.
  virtual void abort(const error& reason) = 0;
};

} // namespace caf::net
