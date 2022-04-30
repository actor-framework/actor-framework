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
class prometheus_acceptor_factory {
public:
  using state_ptr = net::prometheus::server::scrape_state_ptr;

  explicit prometheus_acceptor_factory(state_ptr ptr) : ptr_(std::move(ptr)) {
    // nop
  }

  error init(net::socket_manager*, const settings&) {
    return none;
  }

  template <class Socket>
  net::socket_manager_ptr make(net::multiplexer* mpx, Socket fd) {
    auto prom_serv = net::prometheus::server::make(ptr_);
    auto http_serv = net::http::server::make(std::move(prom_serv));
    auto transport = Transport::make(fd, std::move(http_serv));
    return net::socket_manager::make(mpx, fd, std::move(transport));
  }

  void abort(const error&) {
    // nop
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
disposable serve(actor_system& sys, Socket fd) {
  using factory_t = detail::prometheus_acceptor_factory<Transport>;
  using impl_t = connection_acceptor<Socket, factory_t>;
  auto mpx = &sys.network_manager().mpx();
  auto registry = &sys.metrics();
  auto state = prometheus::server::scrape_state::make(registry);
  auto factory = factory_t{std::move(state)};
  auto impl = impl_t::make(fd, 0, std::move(factory));
  auto mgr = socket_manager::make(mpx, fd, std::move(impl));
  mpx->init(mgr);
  return mgr->make_disposer();
}

} // namespace caf::net::prometheus
