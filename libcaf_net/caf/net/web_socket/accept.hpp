// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/actor_system.hpp"
#include "caf/cow_tuple.hpp"
#include "caf/net/flow_connector.hpp"
#include "caf/net/web_socket/default_trait.hpp"
#include "caf/net/web_socket/flow_bridge.hpp"
#include "caf/net/web_socket/flow_connector_request_impl.hpp"
#include "caf/net/web_socket/frame.hpp"
#include "caf/net/web_socket/request.hpp"
#include "caf/net/web_socket/server.hpp"

#include <type_traits>

namespace caf::detail {

template <class Transport, class Trait>
class ws_acceptor_factory
  : public net::connection_factory<typename Transport::socket_type> {
public:
  using socket_type = typename Transport::socket_type;

  using connector_pointer = net::flow_connector_ptr<Trait>;

  explicit ws_acceptor_factory(connector_pointer connector)
    : connector_(std::move(connector)) {
    // nop
  }

  net::socket_manager_ptr make(net::multiplexer* mpx, socket_type fd) override {
    auto app = net::web_socket::flow_bridge<Trait>::make(mpx, connector_);
    auto app_ptr = app.get();
    auto ws = net::web_socket::server::make(std::move(app));
    auto transport = Transport::make(fd, std::move(ws));
    auto mgr = net::socket_manager::make(mpx, std::move(transport));
    app_ptr->self_ref(mgr->as_disposable());
    return mgr;
  }

private:
  connector_pointer connector_;
};

} // namespace caf::detail

namespace caf::net::web_socket {

/// Describes the per-connection event.
template <class... Ts>
using accept_event_t
  = cow_tuple<async::consumer_resource<frame>, // Socket to App.
              async::producer_resource<frame>, // App to Socket.
              Ts...>;                          // Handshake data.

/// A producer resource for the acceptor. Any accepted WebSocket connection is
/// represented by two buffers. The user-defined types `Ts...` allow the
/// @ref request to transfer additional context for the connection to the
/// listener (usually extracted from WebSocket handshake fields).
template <class... Ts>
using acceptor_resource_t = async::producer_resource<accept_event_t<Ts...>>;

/// Convenience function for creating an event listener resource and an
/// @ref acceptor_resource_t via @ref async::make_spsc_buffer_resource.
template <class... Ts>
auto make_accept_event_resources() {
  return async::make_spsc_buffer_resource<accept_event_t<Ts...>>();
}

/// Listens for incoming WebSocket connection on @p fd.
/// @param sys The host system.
/// @param fd An accept socket in listening mode. For a TCP socket, this socket
///           must already listen to a port.
/// @param out A buffer resource that connects the server to a listener that
///            processes the buffer pairs for each incoming connection.
/// @param on_request Function object for accepting incoming requests.
/// @param cfg Configuration parameters for the acceptor.
template <class Transport = stream_transport, class Socket, class... Ts,
          class OnRequest>
disposable accept(actor_system& sys, Socket fd, acceptor_resource_t<Ts...> out,
                  OnRequest on_request, const settings& cfg = {}) {
  using trait_t = default_trait;
  using request_t = request<default_trait, Ts...>;
  static_assert(std::is_invocable_v<OnRequest, const settings&, request_t&>,
                "invalid signature found for on_request");
  using factory_t = detail::ws_acceptor_factory<Transport, trait_t>;
  using impl_t = connection_acceptor<Socket>;
  using conn_t = flow_connector_request_impl<OnRequest, trait_t, Ts...>;
  auto max_connections = get_or(cfg, defaults::net::max_connections);
  if (auto buf = out.try_open()) {
    auto& mpx = sys.network_manager().mpx();
    auto conn = std::make_shared<conn_t>(std::move(on_request), std::move(buf));
    auto factory = std::make_unique<factory_t>(std::move(conn));
    auto impl = impl_t::make(fd, std::move(factory), max_connections);
    auto impl_ptr = impl.get();
    auto ptr = socket_manager::make(&mpx, std::move(impl));
    impl_ptr->self_ref(ptr->as_disposable());
    mpx.start(ptr);
    return disposable{std::move(ptr)};
  } else {
    return {};
  }
}

} // namespace caf::net::web_socket
