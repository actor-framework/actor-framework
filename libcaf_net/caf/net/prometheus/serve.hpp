// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/actor_system.hpp"
#include "caf/fwd.hpp"
#include "caf/net/http/server.hpp"
#include "caf/net/prometheus/server.hpp"
#include "caf/net/stream_transport.hpp"

namespace caf::detail {

template <class Transport>
class prometheus_acceptor_factory
  : public net::connection_factory<typename Transport::socket_type> {
public:
  using state_ptr = net::prometheus::server::scrape_state_ptr;

  using socket_type = typename Transport::socket_type;

  explicit prometheus_acceptor_factory(state_ptr ptr) : ptr_(std::move(ptr)) {
    // nop
  }

  net::socket_manager_ptr make(net::multiplexer* mpx, socket_type fd) override {
    auto prom_serv = net::prometheus::server::make(ptr_);
    auto http_serv = net::http::server::make(std::move(prom_serv));
    auto transport = Transport::make(fd, std::move(http_serv));
    return net::socket_manager::make(mpx, std::move(transport));
  }

private:
  state_ptr ptr_;
};

} // namespace caf::detail

namespace caf::net::prometheus {

/// Listens for incoming WebSocket connection on @p fd.
/// @param sys The host system.
/// @param fd An accept socket in listening mode. For a TCP socket, this socket
///           must already listen to a port.
template <class Transport = stream_transport, class Socket>
disposable serve(actor_system& sys, Socket fd, const settings& cfg = {}) {
  using factory_t = detail::prometheus_acceptor_factory<Transport>;
  using impl_t = connection_acceptor<Socket>;
  auto mpx = &sys.network_manager().mpx();
  auto registry = &sys.metrics();
  auto state = prometheus::server::scrape_state::make(registry);
  auto factory = std::make_unique<factory_t>(std::move(state));
  auto max_connections = get_or(cfg, defaults::net::max_connections);
  auto impl = impl_t::make(fd, std::move(factory), max_connections);
  auto mgr = socket_manager::make(mpx, std::move(impl));
  mpx->start(mgr);
  return mgr->as_disposable();
}

} // namespace caf::net::prometheus
