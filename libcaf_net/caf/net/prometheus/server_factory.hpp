// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/actor_system.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/accept_handler.hpp"
#include "caf/detail/connection_factory.hpp"
#include "caf/detail/shared_ssl_acceptor.hpp"
#include "caf/fwd.hpp"
#include "caf/net/checked_socket.hpp"
#include "caf/net/dsl/server_config.hpp"
#include "caf/net/dsl/server_factory_base.hpp"
#include "caf/net/http/server.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/prometheus/server.hpp"
#include "caf/net/ssl/transport.hpp"
#include "caf/net/stream_transport.hpp"
#include "caf/net/tcp_accept_socket.hpp"
#include "caf/none.hpp"

#include <cstdint>
#include <functional>
#include <variant>

namespace caf::detail {

template <class Transport>
class prom_conn_factory
  : public connection_factory<typename Transport::connection_handle> {
public:
  using connection_handle = typename Transport::connection_handle;

  using state_ptr = net::prometheus::server::scrape_state_ptr;

  explicit prom_conn_factory(state_ptr ptr) : ptr_(std::move(ptr)) {
    // nop
  }

  net::socket_manager_ptr make(net::multiplexer* mpx,
                               connection_handle conn) override {
    auto prom_serv = net::prometheus::server::make(ptr_);
    auto http_serv = net::http::server::make(std::move(prom_serv));
    auto transport = Transport::make(std::move(conn), std::move(http_serv));
    return net::socket_manager::make(mpx, std::move(transport));
  }

private:
  state_ptr ptr_;
};

} // namespace caf::detail

namespace caf::net::prometheus {

/// Entry point for the `with(...).accept(...).start()` DSL.
class server_factory
  : public dsl::server_factory_base<dsl::config_base, server_factory> {
public:
  using super = dsl::server_factory_base<dsl::config_base, server_factory>;

  using config_type = typename super::config_type;

  using super::super;

  /// Starts the Prometheus service in the background.
  [[nodiscard]] expected<disposable> start() {
    auto& cfg = super::config();
    return cfg.visit([this, &cfg](auto& data) {
      return do_start(cfg, data).or_else([&cfg](const error& err) { //
        cfg.call_on_error(err);
      });
    });
  }

private:
  template <class Acceptor>
  expected<disposable> do_start_impl(config_type& cfg, Acceptor acc) {
    using transport_t = typename Acceptor::transport_type;
    using factory_t = detail::prom_conn_factory<transport_t>;
    using impl_t = detail::accept_handler<Acceptor>;
    auto* mpx = cfg.mpx;
    auto* registry = &mpx->system().metrics();
    auto state = prometheus::server::scrape_state::make(registry);
    auto factory = std::make_unique<factory_t>(std::move(state));
    auto impl = impl_t::make(std::forward<Acceptor>(acc), std::move(factory),
                             cfg.max_connections);
    auto mgr = socket_manager::make(mpx, std::move(impl));
    mpx->start(mgr);
    return expected<disposable>{mgr->as_disposable()};
  }

  expected<disposable> do_start(config_type& cfg,
                                dsl::server_config::socket& data) {
    return checked_socket(data.take_fd())
      .and_then(data.acceptor_with_ctx([this, &cfg](auto& acc) {
        return this->do_start_impl(cfg, std::move(acc));
      }));
  }

  expected<disposable> do_start(config_type& cfg,
                                dsl::server_config::lazy& data) {
    return make_tcp_accept_socket(data.port, data.bind_address, data.reuse_addr)
      .and_then(data.acceptor_with_ctx([this, &cfg](auto& acc) {
        return this->do_start_impl(cfg, std::move(acc));
      }));
  }

  expected<disposable> do_start(config_type&, error& err) {
    return expected<disposable>{std::move(err)};
  }
};

} // namespace caf::net::prometheus
