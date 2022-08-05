// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/web_socket/connect.hpp"

#include "caf/net/flow_connector.hpp"
#include "caf/net/middleman.hpp"
#include "caf/net/ssl/context.hpp"
#include "caf/net/ssl/transport.hpp"
#include "caf/net/stream_transport.hpp"
#include "caf/net/tcp_stream_socket.hpp"
#include "caf/net/web_socket/client.hpp"
#include "caf/net/web_socket/default_trait.hpp"
#include "caf/net/web_socket/flow_bridge.hpp"

namespace ws = caf::net::web_socket;

namespace caf::detail {

template <class Transport, class Connection>
ws::connect_state
ws_do_connect_impl(actor_system& sys, Connection conn, ws::handshake& hs) {
  using trait_t = ws::default_trait;
  using connector_t = net::flow_connector_trivial_impl<trait_t>;
  auto [ws_pull, app_push] = async::make_spsc_buffer_resource<ws::frame>();
  auto [app_pull, ws_push] = async::make_spsc_buffer_resource<ws::frame>();
  auto mpx = sys.network_manager().mpx_ptr();
  auto connector = std::make_shared<connector_t>(std::move(ws_pull),
                                                 std::move(ws_push));
  auto bridge = ws::flow_bridge<trait_t>::make(mpx, std::move(connector));
  auto impl = ws::client::make(std::move(hs), std::move(bridge));
  auto fd = conn.fd();
  auto transport = Transport::make(std::move(conn), std::move(impl));
  transport->active_policy().connect(fd);
  auto ptr = net::socket_manager::make(mpx, std::move(transport));
  mpx->start(ptr);
  return ws::connect_state{ptr->as_disposable(),
                           ws::connect_event_t{app_pull, app_push}};
}

ws::connect_state ws_do_connect(actor_system& sys, net::stream_socket fd,
                                ws::handshake& hs) {
  return ws_do_connect_impl<net::stream_transport>(sys, fd, hs);
}

ws::connect_state ws_do_connect(actor_system& sys, net::ssl::connection conn,
                                ws::handshake& hs) {
  return ws_do_connect_impl<net::ssl::transport>(sys, std::move(conn), hs);
}

expected<ws::connect_state> ws_connect_impl(actor_system& sys,
                                            const caf::uri& dst,
                                            ws_handshake_setup& setup) {
  if (dst.scheme() != "ws" && dst.scheme() != "wss") {
    return {make_error(sec::invalid_argument, "URI must use ws or wss scheme")};
  }
  auto fd = net::make_connected_tcp_stream_socket(dst.authority());
  if (!fd) {
    return {std::move(fd.error())};
  }
  net::web_socket::handshake hs;
  hs.host(dst.host_str());
  hs.endpoint(dst.path_query_fragment());
  setup(hs);
  if (dst.scheme() == "ws") {
    return {ws_do_connect(sys, *fd, hs)};
  } else {
    auto ctx = net::ssl::context::make_client(net::ssl::tls::any);
    if (!ctx) {
      return {std::move(ctx.error())};
    }
    auto conn = ctx->new_connection(*fd);
    if (!conn) {
      return {std::move(conn.error())};
    }
    return {ws_do_connect(sys, std::move(*conn), hs)};
  }
}

} // namespace caf::detail

namespace caf::net::web_socket {

expected<connect_state> connect(actor_system& sys, const caf::uri& dst) {
  auto cb = make_callback([](net::web_socket::handshake&) {});
  return detail::ws_connect_impl(sys, dst, cb);
}

} // namespace caf::net::web_socket
