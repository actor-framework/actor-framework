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
#include "caf/net/http/server.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/prometheus/accept_factory.hpp"
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
class prometheus_conn_factory
  : public connection_factory<typename Transport::connection_handle> {
public:
  using state_ptr = net::prometheus::server::scrape_state_ptr;

  using connection_handle = typename Transport::connection_handle;

  explicit prometheus_conn_factory(state_ptr ptr) : ptr_(std::move(ptr)) {
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

class with_t;

/// Entry point for the `with(...).accept(...).start()` DSL.
class accept_factory {
public:
  friend class with_t;

  accept_factory(accept_factory&&) = default;

  accept_factory(const accept_factory&) = delete;

  accept_factory& operator=(accept_factory&&) noexcept = default;

  accept_factory& operator=(const accept_factory&) noexcept = delete;

  ~accept_factory() {
    if (auto* fd = std::get_if<tcp_accept_socket>(&state_))
      close(*fd);
  }

  /// Configures how many concurrent connections we are allowing.
  accept_factory& max_connections(size_t value) {
    max_connections_ = value;
    return *this;
  }

  /// Sets the callback for errors.
  template <class F>
  accept_factory& do_on_error(F callback) {
    do_on_error_ = std::move(callback);
    return *this;
  }

  /// Starts the Prometheus service in the background.
  disposable start() {
    switch (state_.index()) {
      case 1: {
        auto& cfg = std::get<1>(state_);
        auto fd = make_tcp_accept_socket(cfg.port, cfg.address, cfg.reuse_addr);
        if (fd)
          return do_start(*fd);
        if (do_on_error_)
          do_on_error_(fd.error());
        return {};
      }
      case 2: {
        // Pass ownership of the socket to the accept handler.
        auto fd = std::get<2>(state_);
        state_ = none;
        return do_start(fd);
      }
      default:
        return {};
    }
  }

private:
  struct config {
    uint16_t port;
    std::string address;
    bool reuse_addr;
  };

  explicit accept_factory(actor_system* sys) : sys_(sys) {
    // nop
  }

  disposable do_start(tcp_accept_socket fd) {
    if (!ctx_) {
      using factory_t = detail::prometheus_conn_factory<stream_transport>;
      using impl_t = detail::accept_handler<tcp_accept_socket, stream_socket>;
      auto mpx = &sys_->network_manager().mpx();
      auto registry = &sys_->metrics();
      auto state = prometheus::server::scrape_state::make(registry);
      auto factory = std::make_unique<factory_t>(std::move(state));
      auto impl = impl_t::make(fd, std::move(factory), max_connections_);
      auto mgr = socket_manager::make(mpx, std::move(impl));
      mpx->start(mgr);
      return mgr->as_disposable();
    }
    using factory_t = detail::prometheus_conn_factory<ssl::transport>;
    using acc_t = detail::shared_ssl_acceptor;
    using impl_t = detail::accept_handler<acc_t, ssl::connection>;
    auto mpx = &sys_->network_manager().mpx();
    auto registry = &sys_->metrics();
    auto state = prometheus::server::scrape_state::make(registry);
    auto factory = std::make_unique<factory_t>(std::move(state));
    auto impl = impl_t::make(acc_t{fd, ctx_}, std::move(factory),
                             max_connections_);
    auto mgr = socket_manager::make(mpx, std::move(impl));
    mpx->start(mgr);
    return mgr->as_disposable();
  }

  void set_ssl(ssl::context ctx) {
    ctx_ = std::make_shared<ssl::context>(std::move(ctx));
  }

  void init(uint16_t port, std::string address, bool reuse_addr) {
    state_ = config{port, std::move(address), reuse_addr};
  }

  void init(tcp_accept_socket fd) {
    state_ = fd;
  }

  /// Pointer to the hosting actor system.
  actor_system* sys_;

  /// Callback for errors.
  std::function<void(const error&)> do_on_error_;

  /// Configures the maximum number of concurrent connections.
  size_t max_connections_ = defaults::net::max_connections.fallback;

  /// User-defined state for getting things up and running.
  std::variant<none_t, config, tcp_accept_socket> state_;

  std::shared_ptr<ssl::context> ctx_;
};

} // namespace caf::net::prometheus
