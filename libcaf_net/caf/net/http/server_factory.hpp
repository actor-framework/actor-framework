// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/checked_socket.hpp"
#include "caf/net/dsl/generic_config.hpp"
#include "caf/net/dsl/server_factory_base.hpp"
#include "caf/net/http/config.hpp"
#include "caf/net/http/request.hpp"
#include "caf/net/http/router.hpp"
#include "caf/net/http/server.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/octet_stream/transport.hpp"
#include "caf/net/ssl/transport.hpp"
#include "caf/net/tcp_accept_socket.hpp"
#include "caf/net/tcp_stream_socket.hpp"

#include "caf/async/blocking_producer.hpp"
#include "caf/async/execution_context.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/accept_handler.hpp"
#include "caf/detail/connection_factory.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"
#include "caf/none.hpp"

#include <cstdint>
#include <memory>

namespace caf::detail {

class CAF_NET_EXPORT http_request_producer : public async::producer {
public:
  ~http_request_producer() override;

  virtual bool push(const net::http::request& item) = 0;
};

using http_request_producer_ptr = intrusive_ptr<http_request_producer>;

CAF_NET_EXPORT
http_request_producer_ptr
make_http_request_producer(async::execution_context_ptr ecp,
                           async::spsc_buffer_ptr<net::http::request> buf);

CAF_NET_EXPORT connection_acceptor_ptr make_http_conn_acceptor(
  net::tcp_accept_socket fd, std::vector<net::http::route_ptr> routes,
  size_t max_consecutive_reads, size_t max_request_size);

CAF_NET_EXPORT connection_acceptor_ptr make_http_conn_acceptor(
  net::ssl::tcp_acceptor acceptor, std::vector<net::http::route_ptr> routes,
  size_t max_consecutive_reads, size_t max_request_size);

} // namespace caf::detail

namespace caf::net::http {

/// Factory type for the `with(...).accept(...).start(...)` DSL.
class server_factory
  : public dsl::server_factory_base<server_config, server_factory> {
public:
  using super = dsl::server_factory_base<server_config, server_factory>;

  using config_type = typename super::config_type;

  using super::super;

  /// Sets the maximum request size to @p value.
  server_factory& max_request_size(size_t value) noexcept {
    super::config().max_request_size = value;
    return *this;
  }

  /// Monitors the actor handle @p hdl and stops the server if the monitored
  /// actor terminates.
  template <class ActorHandle>
  server_factory& monitor(const ActorHandle& hdl) {
    auto& cfg = super::config();
    auto ptr = actor_cast<strong_actor_ptr>(hdl);
    if (!ptr) {
      auto err = make_error(sec::logic_error,
                            "cannot monitor an invalid actor handle");
      cfg.fail(std::move(err));
      return *this;
    }
    cfg.monitored_actors.push_back(std::move(ptr));
    return *this;
  }

  /// Adds a new route to the HTTP server.
  /// @param path The path on this server for the new route.
  /// @param f The function object for handling requests on the new route.
  /// @return a reference to `*this`.
  template <class F>
  server_factory& route(std::string path, F f) {
    auto& cfg = super::config();
    if (cfg.failed())
      return *this;
    auto new_route = make_route(std::move(path), std::move(f));
    if (!new_route) {
      cfg.fail(std::move(new_route.error()));
    } else {
      cfg.routes.push_back(std::move(*new_route));
    }
    return *this;
  }

  /// Adds a new route to the HTTP server.
  /// @param path The path on this server for the new route.
  /// @param method The allowed HTTP method on the new route.
  /// @param f The function object for handling requests on the new route.
  /// @return a reference to `*this`.
  template <class F>
  server_factory& route(std::string path, http::method method, F f) {
    auto& cfg = super::config();
    if (cfg.failed())
      return *this;
    auto new_route = make_route(std::move(path), method, std::move(f));
    if (!new_route) {
      cfg.fail(std::move(new_route.error()));
    } else {
      cfg.routes.push_back(std::move(*new_route));
    }
    return *this;
  }

  /// Starts a server that makes HTTP requests without a fixed route available
  /// to an observer.
  template <class OnStart>
  [[nodiscard]] expected<disposable> start(OnStart on_start) {
    using consumer_resource = async::consumer_resource<request>;
    static_assert(std::is_invocable_v<OnStart, consumer_resource>);
    for (auto& ptr : super::config().routes)
      ptr->init();
    auto& cfg = super::config();
    return cfg.visit([this, &cfg, &on_start](auto& data) {
      return this->do_start(cfg, data, on_start)
        .or_else([&cfg](const error& err) { cfg.call_on_error(err); });
    });
  }

  /// Starts a server that only serves the fixed routes.
  [[nodiscard]] expected<disposable> start() {
    unit_t dummy;
    return start(dummy);
  }

private:
  template <class Acceptor>
  expected<disposable> do_start_impl(config_type& cfg, Acceptor acc, unit_t&) {
    if (cfg.routes.empty()) {
      return make_error(sec::logic_error,
                        "cannot start an HTTP server without any routes");
    }
    auto factory = detail::make_http_conn_acceptor(std::move(acc), cfg.routes,
                                                   cfg.max_consecutive_reads,
                                                   cfg.max_request_size);
    auto impl = detail::make_accept_handler(std::move(factory),
                                            cfg.max_connections,
                                            cfg.monitored_actors);
    auto ptr = net::socket_manager::make(cfg.mpx, std::move(impl));
    cfg.mpx->start(ptr);
    return expected<disposable>{disposable{std::move(ptr)}};
  }

  template <class Acceptor, class OnStart>
  expected<disposable>
  do_start_impl(config_type& cfg, Acceptor acc, OnStart& on_start) {
    auto routes = cfg.routes;
    auto [pull, push] = async::make_spsc_buffer_resource<request>();
    auto producer = detail::make_http_request_producer(cfg.mpx,
                                                       push.try_open());
    routes.push_back(make_route([producer](responder& res) {
      if (!producer->push(responder{res}.to_request())) {
        auto err = make_error(sec::runtime_error, "flow disconnected");
        res.router()->shutdown(err);
      }
    }));
    auto factory = detail::make_http_conn_acceptor(std::move(acc),
                                                   std::move(routes),
                                                   cfg.max_consecutive_reads);
    auto impl = detail::make_accept_handler(std::move(factory),
                                            cfg.max_connections,
                                            cfg.monitored_actors);
    auto ptr = net::socket_manager::make(cfg.mpx, std::move(impl));
    cfg.mpx->start(ptr);
    on_start(std::move(pull));
    return expected<disposable>{disposable{std::move(ptr)}};
  }

  template <class OnStart>
  expected<disposable> do_start(config_type& cfg,
                                dsl::server_config::socket& data,
                                OnStart& on_start) {
    return checked_socket(data.take_fd())
      .and_then(
        this->with_ssl_acceptor_or_socket([this, &cfg, &on_start](auto&& acc) {
          using acc_t = std::decay_t<decltype(acc)>;
          return this->do_start_impl(cfg, std::forward<acc_t>(acc), on_start);
        }));
  }

  template <class OnStart>
  expected<disposable> do_start(config_type& cfg,
                                dsl::server_config::lazy& data,
                                OnStart& on_start) {
    return make_tcp_accept_socket(data.port, data.bind_address, data.reuse_addr)
      .and_then(
        this->with_ssl_acceptor_or_socket([this, &cfg, &on_start](auto&& acc) {
          using acc_t = std::decay_t<decltype(acc)>;
          return this->do_start_impl(cfg, std::forward<acc_t>(acc), on_start);
        }));
  }

  template <class OnStart>
  expected<disposable> do_start(config_type&, error& err, OnStart&) {
    return expected<disposable>{std::move(err)};
  }
};

} // namespace caf::net::http
