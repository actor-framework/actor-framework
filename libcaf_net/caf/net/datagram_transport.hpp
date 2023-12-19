// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/actor_shell.hpp"
#include "caf/net/datagram_socket.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/make_actor_shell.hpp"
#include "caf/net/socket_event_layer.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/net/udp_datagram_socket.hpp"

#include "caf/byte_buffer.hpp"
#include "caf/error.hpp"
#include "caf/fwd.hpp"
#include "caf/ip_endpoint.hpp"
#include "caf/log/net.hpp"
#include "caf/logger.hpp"
#include "caf/sec.hpp"

#include <iostream>
#include <vector>

namespace caf::net {

/// Manages a datagram socket.
class CAF_NET_EXPORT datagram_transport : public socket_event_layer {
public:
  // Maximal UDP-packet size.
  static constexpr size_t max_datagram_size
    = std::numeric_limits<uint16_t>::max();

  // -- constructors, destructors, and assignment operators --------------------

  explicit datagram_transport(udp_datagram_socket handle, actor_system& sys,
                              async::execution_context_ptr loop, actor worker)
    : handle_(handle), worker_(std::move(worker)) {
    self = make_actor_shell(sys, loop);
  }

  // -- implementation of socket_event_layer -----------------------------------

  error start(socket_manager* owner) override;

  socket handle() const override;

  void handle_read_event() override;

  void handle_write_event() override;

  void abort(const error& error) override;

  // -- utility functions ------------------------------------------------------

  /// Returns a handle to the actor shell.
  actor actor_handle() const noexcept {
    return self.as_actor();
  }

private:
  // -- member variables -------------------------------------------------------

  /// Pointer to associated socket manager.
  socket_manager* parent_ = nullptr;

  /// Handle for the managed socket.
  udp_datagram_socket handle_;

  /// Caches incoming data.
  byte_buffer read_buf_;

  /// Caches outgoing data.
  byte_buffer write_buf_;

  /// Stores the max number of bytes to receive.
  size_t max_read_size_ = max_datagram_size;

  /// Destination for the outgoing datagrams.
  ip_endpoint dest_;

  /// Worker actor for processing incoming datagrams.
  actor worker_ = {};

  // Actor shell representing this app.
  net::actor_shell_ptr self;
};

} // namespace caf::net
