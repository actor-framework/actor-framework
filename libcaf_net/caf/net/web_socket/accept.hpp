// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/actor_system.hpp"
#include "caf/net/web_socket/default_trait.hpp"
#include "caf/net/web_socket/flow_bridge.hpp"
#include "caf/net/web_socket/flow_connector.hpp"
#include "caf/net/web_socket/frame.hpp"
#include "caf/net/web_socket/request.hpp"
#include "caf/net/web_socket/server.hpp"

#include <type_traits>

namespace caf::detail {

template <template <class> class Transport, class Trait>
class ws_acceptor_factory {
public:
  using connector_pointer = net::web_socket::flow_connector_ptr<Trait>;

  explicit ws_acceptor_factory(connector_pointer connector)
    : connector_(std::move(connector)) {
    // nop
  }

  error init(net::socket_manager*, const settings&) {
    return none;
  }

  template <class Socket>
  net::socket_manager_ptr make(Socket fd, net::multiplexer* mpx) {
    using app_t = net::web_socket::flow_bridge<Trait>;
    using stack_t = Transport<net::web_socket::server<app_t>>;
    return net::make_socket_manager<stack_t>(fd, mpx, connector_);
  }

  void abort(const error&) {
    // nop
  }

private:
  connector_pointer connector_;
};

} // namespace caf::detail

namespace caf::net::web_socket {

/// Listens for incoming WebSocket connection on @p fd.
/// @param sys The host system.
/// @param fd An accept socket in listening mode. For a TCP socket, this socket
///           must already listen to a port.
/// @param res A buffer resource that connects the server to a listener that
///            processes the buffer pairs for each incoming connection.
/// @param on_request Function object for accepting incoming requests.
/// @param limit The maximum amount of connections before closing @p fd. Passing
///              0 means "no limit".
template <template <class> class Transport = stream_transport, class Socket,
          class OnRequest>
void accept(actor_system& sys, Socket fd,
            async::producer_resource<async::resource_pair<frame>> res,
            OnRequest on_request, size_t limit = 0) {
  using trait_t = default_trait;
  using request_t = request<default_trait>;
  static_assert(std::is_invocable_v<OnRequest, const settings&, request_t&>,
                "invalid signature found for on_request");
  using factory_t = detail::ws_acceptor_factory<Transport, trait_t>;
  using impl_t = connection_acceptor<Socket, factory_t>;
  using conn_t = flow_connector_impl<OnRequest, trait_t>;
  if (auto buf = res.try_open()) {
    auto& mpx = sys.network_manager().mpx();
    auto conn = std::make_shared<conn_t>(std::move(buf), std::move(on_request));
    auto factory = factory_t{std::move(conn)};
    auto ptr = make_socket_manager<impl_t>(std::move(fd), &mpx, limit,
                                           std::move(factory));
    mpx.init(ptr);
  }
  // TODO: accept() should return a disposable to stop the WebSocket server.
}

} // namespace caf::net::web_socket
