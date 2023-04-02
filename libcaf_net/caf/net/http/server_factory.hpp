// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/blocking_producer.hpp"
#include "caf/async/execution_context.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/accept_handler.hpp"
#include "caf/detail/connection_factory.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"
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
#include "caf/none.hpp"

#include <cstdint>
#include <memory>

namespace caf::detail {

class CAF_NET_EXPORT http_request_producer : public atomic_ref_counted,
                                             public async::producer {
public:
  using buffer_ptr = async::spsc_buffer_ptr<net::http::request>;

  http_request_producer(async::execution_context_ptr ecp, buffer_ptr buf)
    : ecp_(std::move(ecp)), buf_(std::move(buf)) {
    // nop
  }

  static auto make(async::execution_context_ptr ecp, buffer_ptr buf) {
    auto ptr = make_counted<http_request_producer>(std::move(ecp), buf);
    buf->set_producer(ptr);
    return ptr;
  }

  void on_consumer_ready() override;

  void on_consumer_cancel() override;

  void on_consumer_demand(size_t) override;

  void ref_producer() const noexcept override;

  void deref_producer() const noexcept override;

  bool push(const net::http::request& item);

private:
  async::execution_context_ptr ecp_;
  buffer_ptr buf_;
};

using http_request_producer_ptr = intrusive_ptr<http_request_producer>;

template <class Transport>
class http_conn_factory
  : public connection_factory<typename Transport::connection_handle> {
public:
  using connection_handle = typename Transport::connection_handle;

  http_conn_factory(std::vector<net::http::route_ptr> routes,
                    size_t max_consecutive_reads)
    : routes_(std::move(routes)),
      max_consecutive_reads_(max_consecutive_reads) {
    // nop
  }

  net::socket_manager_ptr make(net::multiplexer* mpx,
                               connection_handle conn) override {
    auto app = net::http::router::make(routes_);
    auto serv = net::http::server::make(std::move(app));
    auto fd = conn.fd();
    auto transport = Transport::make(std::move(conn), std::move(serv));
    transport->max_consecutive_reads(max_consecutive_reads_);
    transport->active_policy().accept(fd);
    auto res = net::socket_manager::make(mpx, std::move(transport));
    mpx->watch(res->as_disposable());
    return res;
  }

private:
  std::vector<net::http::route_ptr> routes_;
  size_t max_consecutive_reads_;
  action monitor_;
};

} // namespace caf::detail

namespace caf::net::http {

/// Factory type for the `with(...).accept(...).start(...)` DSL.
class server_factory
  : public dsl::server_factory_base<server_config, server_factory> {
public:
  using super = dsl::server_factory_base<server_config, server_factory>;

  using config_type = typename super::config_type;

  using super::super;

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
    using transport_t = typename Acceptor::transport_type;
    using factory_t = detail::http_conn_factory<transport_t>;
    using impl_t = detail::accept_handler<Acceptor>;
    auto factory = std::make_unique<factory_t>(cfg.routes,
                                               cfg.max_consecutive_reads);
    auto impl = impl_t::make(std::move(acc), std::move(factory),
                             cfg.max_connections, cfg.monitored_actors);
    auto impl_ptr = impl.get();
    auto ptr = net::socket_manager::make(cfg.mpx, std::move(impl));
    impl_ptr->self_ref(ptr->as_disposable());
    cfg.mpx->start(ptr);
    return expected<disposable>{disposable{std::move(ptr)}};
  }

  template <class Acceptor, class OnStart>
  expected<disposable>
  do_start_impl(config_type& cfg, Acceptor acc, OnStart& on_start) {
    using transport_t = typename Acceptor::transport_type;
    using factory_t = detail::http_conn_factory<transport_t>;
    using impl_t = detail::accept_handler<Acceptor>;
    auto routes = cfg.routes;
    auto [pull, push] = async::make_spsc_buffer_resource<request>();
    auto producer = detail::http_request_producer::make(cfg.mpx,
                                                        push.try_open());
    routes.push_back(make_route([producer](responder& res) {
      if (!producer->push(std::move(res).to_request())) {
        auto err = make_error(sec::runtime_error, "flow disconnected");
        res.router()->shutdown(err);
      }
    }));
    auto factory = std::make_unique<factory_t>(std::move(routes),
                                               cfg.max_consecutive_reads);
    auto impl = impl_t::make(std::move(acc), std::move(factory),
                             cfg.max_connections, cfg.monitored_actors);
    auto impl_ptr = impl.get();
    auto ptr = net::socket_manager::make(cfg.mpx, std::move(impl));
    impl_ptr->self_ref(ptr->as_disposable());
    cfg.mpx->start(ptr);
    on_start(std::move(pull));
    return expected<disposable>{disposable{std::move(ptr)}};
  }

  template <class OnStart>
  expected<disposable> do_start(config_type& cfg,
                                dsl::server_config::socket& data,
                                OnStart& on_start) {
    return checked_socket(data.take_fd())
      .and_then(data.acceptor_with_ctx([this, &cfg, &on_start](auto& acc) {
        return this->do_start_impl(cfg, std::move(acc), on_start);
      }));
  }

  template <class OnStart>
  expected<disposable> do_start(config_type& cfg,
                                dsl::server_config::lazy& data,
                                OnStart& on_start) {
    return make_tcp_accept_socket(data.port, data.bind_address, data.reuse_addr)
      .and_then(data.acceptor_with_ctx([this, &cfg, &on_start](auto& acc) {
        return this->do_start_impl(cfg, std::move(acc), on_start);
      }));
  }

  template <class OnStart>
  expected<disposable> do_start(config_type&, error& err, OnStart&) {
    return expected<disposable>{std::move(err)};
  }
};

} // namespace caf::net::http
