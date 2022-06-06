// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/stream_transport.hpp"
#include "caf/net/web_socket/client.hpp"
#include "caf/net/web_socket/default_trait.hpp"
#include "caf/net/web_socket/flow_bridge.hpp"
#include "caf/net/web_socket/frame.hpp"
#include "caf/net/web_socket/handshake.hpp"

namespace caf::net::web_socket {

/// Describes the one-time connection event.
using connect_event_t
  = cow_tuple<async::consumer_resource<frame>,  // Socket to App.
              async::producer_resource<frame>>; // App to Socket.

/// Tries to establish a WebSocket connection on @p fd.
/// @param sys The host system.
/// @param fd An connected socket.
/// @param init Function object for setting up the created flows.
template <class Transport = stream_transport, class Socket, class Init>
void connect(actor_system& sys, Socket fd, handshake hs, Init init) {
  // TODO: connect() should return a disposable to stop the WebSocket.
  using trait_t = default_trait;
  static_assert(std::is_invocable_v<Init, connect_event_t&&>,
                "invalid signature found for init");
  auto [ws_pull, app_push] = async::make_spsc_buffer_resource<frame>();
  auto [app_pull, ws_push] = async::make_spsc_buffer_resource<frame>();
  using conn_t = flow_connector_trivial_impl<trait_t>;
  auto mpx = sys.network_manager().mpx_ptr();
  auto conn = std::make_shared<conn_t>(std::move(ws_pull), std::move(ws_push));
  auto bridge = flow_bridge<trait_t>::make(mpx, std::move(conn));
  auto impl = client::make(std::move(hs), std::move(bridge));
  auto transport = Transport::make(fd, std::move(impl));
  auto ptr = socket_manager::make(mpx, std::move(transport));
  mpx->start(ptr);
  init(connect_event_t{app_pull, app_push});
}

} // namespace caf::net::web_socket
