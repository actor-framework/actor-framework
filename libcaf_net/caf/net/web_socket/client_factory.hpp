// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/spsc_buffer.hpp"
#include "caf/detail/ws_flow_bridge.hpp"
#include "caf/disposable.hpp"
#include "caf/net/checked_socket.hpp"
#include "caf/net/dsl/client_factory_base.hpp"
#include "caf/net/ssl/connection.hpp"
#include "caf/net/tcp_stream_socket.hpp"
#include "caf/net/web_socket/client.hpp"
#include "caf/net/web_socket/framing.hpp"
#include "caf/timespan.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <variant>

namespace caf::detail {

/// Specializes the WebSocket flow bridge for the server side.
template <class Trait, class... Ts>
class ws_client_flow_bridge
  : public ws_flow_bridge<Trait, net::web_socket::upper_layer> {
public:
  using super = ws_flow_bridge<Trait, net::web_socket::upper_layer>;

  // We consume the output type of the application.
  using pull_t = async::consumer_resource<typename Trait::input_type>;

  // We produce the input type of the application.
  using push_t = async::producer_resource<typename Trait::output_type>;

  ws_client_flow_bridge(pull_t pull, push_t push)
    : pull_(std::move(pull)), push_(std::move(push)) {
    // nop
  }

  static std::unique_ptr<ws_client_flow_bridge> make(pull_t pull, push_t push) {
    return std::make_unique<ws_client_flow_bridge>(std::move(pull),
                                                   std::move(push));
  }

  error start(net::web_socket::lower_layer* down_ptr) override {
    super::down_ = down_ptr;
    return super::init(&down_ptr->mpx(), std::move(pull_), std::move(push_));
  }

private:
  pull_t pull_;
  push_t push_;
};

} // namespace caf::detail

namespace caf::net::web_socket {

/// Configuration type for WebSocket clients with a handshake object. The
/// handshake object sets the default endpoint to '/' for convenience.
template <class Trait>
class client_factory_config : public dsl::config_with_trait<Trait> {
public:
  using super = dsl::config_with_trait<Trait>;

  explicit client_factory_config(multiplexer* mpx) : super(mpx) {
    hs.endpoint("/");
  }

  explicit client_factory_config(const super& other) : super(other) {
    hs.endpoint("/");
  }

  client_factory_config(const client_factory_config&) = default;

  handshake hs;
};

/// Factory for the `with(...).connect(...).start(...)` DSL.
template <class Trait>
class client_factory
  : public dsl::client_factory_base<client_factory_config<Trait>,
                                    client_factory<Trait>> {
public:
  using super = dsl::client_factory_base<client_factory_config<Trait>,
                                         client_factory<Trait>>;

  using super::super;

  using config_type = typename super::config_type;

  /// Starts a connection with the length-prefixing protocol.
  template <class OnStart>
  [[nodiscard]] expected<disposable> start(OnStart on_start) {
    using input_res_t = typename Trait::input_resource;
    using output_res_t = typename Trait::output_resource;
    static_assert(std::is_invocable_v<OnStart, input_res_t, output_res_t>);
    return super::config().visit([this, &on_start](auto& data) {
      return do_start(super::config(), data, on_start);
    });
  }

private:
  template <class Conn, class OnStart>
  expected<disposable>
  do_start_impl(config_type& cfg, Conn conn, OnStart& on_start) {
    // s2a: socket-to-application (and a2s is the inverse).
    using input_t = typename Trait::input_type;
    using output_t = typename Trait::output_type;
    using transport_t = typename Conn::transport_type;
    using bridge_t = detail::ws_client_flow_bridge<Trait>;
    auto [s2a_pull, s2a_push] = async::make_spsc_buffer_resource<input_t>();
    auto [a2s_pull, a2s_push] = async::make_spsc_buffer_resource<output_t>();
    auto bridge = bridge_t::make(std::move(a2s_pull), std::move(s2a_push));
    auto bridge_ptr = bridge.get();
    auto impl = client::make(std::move(cfg.hs), std::move(bridge));
    auto fd = conn.fd();
    auto transport = transport_t::make(std::move(conn), std::move(impl));
    transport->active_policy().connect(fd);
    auto ptr = socket_manager::make(cfg.mpx, std::move(transport));
    bridge_ptr->self_ref(ptr->as_disposable());
    cfg.mpx->start(ptr);
    on_start(std::move(s2a_pull), std::move(a2s_push));
    return expected<disposable>{disposable{std::move(ptr)}};
  }

  expected<void> sanity_check(config_type& cfg) {
    if (cfg.hs.has_mandatory_fields()) {
      return {};
    } else {
      auto err = make_error(sec::invalid_argument,
                            "WebSocket handshake lacks mandatory fields");
      cfg.call_on_error(err);
      return {std::move(err)};
    }
  }

  template <class OnStart>
  expected<disposable> do_start(config_type& cfg,
                                dsl::client_config::lazy& data,
                                dsl::server_address& addr, OnStart& on_start) {
    cfg.hs.host(addr.host);
    return detail::tcp_try_connect(std::move(addr.host), addr.port,
                                   data.connection_timeout,
                                   data.max_retry_count, data.retry_delay)
      .and_then(data.connection_with_ctx([this, &cfg, &on_start](auto& conn) {
        return this->do_start_impl(cfg, std::move(conn), on_start);
      }));
  }

  template <class OnStart>
  expected<disposable> do_start(config_type& cfg,
                                dsl::client_config::lazy& data, const uri& addr,
                                OnStart& on_start) {
    const auto& auth = addr.authority();
    auto host = auth.host_str();
    auto port = auth.port;
    // Sanity checking.
    if (host.empty()) {
      auto err = make_error(sec::invalid_argument,
                            "URI must provide a valid hostname");
      return do_start(cfg, err, on_start);
    }
    if (addr.scheme() == "ws") {
      if (data.ctx) {
        auto err = make_error(sec::logic_error,
                              "found SSL config with scheme ws");
        return do_start(cfg, err, on_start);
      }
      if (port == 0)
        port = defaults::net::http_default_port;
    } else if (addr.scheme() == "wss") {
      if (port == 0)
        port = defaults::net::https_default_port;
      if (!data.ctx) { // Auto-initialize SSL context for wss.
        auto ctx = ssl::context::make_client(ssl::tls::v1_0);
        if (!ctx)
          return do_start(cfg, ctx.error(), on_start);
        data.ctx = std::make_shared<ssl::context>(std::move(*ctx));
      }
    } else {
      auto err = make_error(sec::invalid_argument,
                            "URI must use ws or wss scheme");
      return do_start(cfg, err, on_start);
    }
    // Fill the handshake with fields from the URI.
    cfg.hs.host(host);
    cfg.hs.endpoint(addr.path_query_fragment());
    // Try to connect.
    return detail::tcp_try_connect(std::move(host), port,
                                   data.connection_timeout,
                                   data.max_retry_count, data.retry_delay)
      .and_then(data.connection_with_ctx([this, &cfg, &on_start](auto& conn) {
        return this->do_start_impl(cfg, std::move(conn), on_start);
      }));
  }

  template <class OnStart>
  expected<disposable> do_start(config_type& cfg,
                                dsl::client_config::lazy& data,
                                OnStart& on_start) {
    auto fn = [this, &cfg, &data, &on_start](auto& addr) {
      return this->do_start(cfg, data, addr, on_start);
    };
    return std::visit(fn, data.server);
  }

  template <class OnStart>
  expected<disposable> do_start(config_type& cfg,
                                dsl::client_config::socket& data,
                                OnStart& on_start) {
    return sanity_check(cfg)
      .transform([&data] { return data.take_fd(); })
      .and_then(check_socket)
      .and_then(data.connection_with_ctx([this, &cfg, &on_start](auto& conn) {
        return this->do_start_impl(cfg, std::move(conn), on_start);
      }));
  }

  template <class OnStart>
  expected<disposable> do_start(config_type& cfg,
                                dsl::client_config::conn& data,
                                OnStart& on_start) {
    return sanity_check(cfg).and_then([&] { //
      return do_start_impl(cfg, std::move(data.state), on_start);
    });
  }

  template <class OnStart>
  expected<disposable> do_start(config_type& cfg, error& err, OnStart&) {
    cfg.call_on_error(err);
    return expected<disposable>{std::move(err)};
  }
};

} // namespace caf::net::web_socket
