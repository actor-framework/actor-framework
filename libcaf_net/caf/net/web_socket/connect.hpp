// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/spsc_buffer.hpp"
#include "caf/callback.hpp"
#include "caf/cow_tuple.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/disposable.hpp"
#include "caf/expected.hpp"
#include "caf/net/ssl/connection.hpp"
#include "caf/net/stream_transport.hpp"
#include "caf/net/web_socket/frame.hpp"
#include "caf/net/web_socket/handshake.hpp"
#include "caf/uri.hpp"

#include <utility>

namespace caf::net::web_socket {

/// Describes the one-time connection event.
using connect_event_t
  = cow_tuple<async::consumer_resource<frame>,  // Socket to App.
              async::producer_resource<frame>>; // App to Socket.

/// Wraps a successful connection setup.
class connect_state {
public:
  connect_state() = delete;
  connect_state(connect_state&&) = default;
  connect_state(const connect_state&) = default;
  connect_state& operator=(connect_state&&) = default;
  connect_state& operator=(const connect_state&) = default;

  connect_state(disposable worker, connect_event_t event)
    : worker_(worker), event_(std::move(event)) {
    // nop
  }

  template <class Init>
  disposable run(Init init) {
    init(std::move(event_));
    return worker_;
  }

private:
  disposable worker_;
  connect_event_t event_;
};

} // namespace caf::net::web_socket

namespace caf::detail {

using ws_handshake_setup = callback<void(net::web_socket::handshake&)>;

net::web_socket::connect_state CAF_NET_EXPORT ws_do_connect(
  actor_system& sys, net::stream_socket fd, net::web_socket::handshake& hs);

net::web_socket::connect_state CAF_NET_EXPORT ws_do_connect(
  actor_system& sys, net::ssl::connection conn, net::web_socket::handshake& hs);

expected<net::web_socket::connect_state>
  CAF_NET_EXPORT ws_connect_impl(actor_system& sys, const caf::uri& dst,
                                 ws_handshake_setup& setup);

} // namespace caf::detail

namespace caf::net::web_socket {

/// Starts a WebSocket connection on @p fd.
/// @param sys The parent system.
/// @param fd An connected socket.
/// @param init Function object for setting up the created flows.
template <class Init>
disposable
connect(actor_system& sys, stream_socket fd, handshake hs, Init init) {
  static_assert(std::is_invocable_v<Init, connect_event_t&&>,
                "invalid signature found for init");
  return detail::ws_do_connect(sys, fd, hs).run(std::move(init));
}

/// Starts a WebSocket connection on @p fd.
/// @param sys The parent system.
/// @param conn An established TCP with an TLS connection attached to it.
/// @param init Function object for setting up the created flows.
template <class Init>
disposable
connect(actor_system& sys, ssl::connection conn, handshake hs, Init init) {
  static_assert(std::is_invocable_v<Init, connect_event_t&&>,
                "invalid signature found for init");
  return detail::ws_do_connect(sys, std::move(conn), hs).run(std::move(init));
}

/// Tries to connect to the host from the URI.
/// @param sys The parent system.
/// @param dst Encodes the destination host. URI scheme must be `ws` or `wss`.
/// @note Blocks the caller while trying to establish a TCP connection.
expected<connect_state> connect(actor_system& sys, const caf::uri& dst);

template <class HandshakeSetup>
expected<connect_state>
connect(actor_system& sys, const caf::uri& dst, HandshakeSetup&& setup) {
  static_assert(std::is_invocable_v<HandshakeSetup, handshake&>,
                "invalid signature found for the handshake setup function");
  auto cb = make_callback(std::forward<HandshakeSetup>(setup));
  return detail::ws_connect_impl(sys, dst, cb);
}

} // namespace caf::net::web_socket
