// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/checked_socket.hpp"
#include "caf/net/dsl/client_factory_base.hpp"
#include "caf/net/ssl/connection.hpp"
#include "caf/net/tcp_stream_socket.hpp"
#include "caf/net/web_socket/client.hpp"
#include "caf/net/web_socket/config.hpp"
#include "caf/net/web_socket/framing.hpp"

#include "caf/async/spsc_buffer.hpp"
#include "caf/detail/ws_flow_bridge.hpp"
#include "caf/disposable.hpp"
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

/// Factory for the `with(...).connect(...).start(...)` DSL.
template <class Trait>
class client_factory : public dsl::client_factory_base<client_config<Trait>,
                                                       client_factory<Trait>> {
public:
  using super
    = dsl::client_factory_base<client_config<Trait>, client_factory<Trait>>;

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
    auto transport = transport_t::make(std::move(conn), std::move(impl));
    transport->active_policy().connect();
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
      auto err = error{sec::invalid_argument,
                       "WebSocket handshake lacks mandatory fields"};
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
      .and_then(this->with_ssl_connection_or_socket(
        [this, &cfg, &on_start](auto&& conn) {
          using conn_t = decltype(conn);
          return this->do_start_impl(cfg, std::forward<conn_t>(conn), on_start);
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
      auto err = error{sec::invalid_argument,
                       "URI must provide a valid hostname"};
      return do_start(cfg, err, on_start);
    }
    // Spin up the server based on the scheme.
    auto use_ssl = false;
    if (addr.scheme() == "ws") {
      if (port == 0)
        port = defaults::net::http_default_port;
    } else if (addr.scheme() == "wss") {
      if (port == 0)
        port = defaults::net::https_default_port;
      use_ssl = true;
    } else {
      auto err = error{sec::invalid_argument,
                       "unsupported URI scheme: expected ws or wss"};
      return do_start(cfg, err, on_start);
    }
    // Fill the handshake with fields from the URI and try to connect.
    cfg.hs.host(host);
    cfg.hs.endpoint(addr.path_query_fragment());
    return detail::tcp_try_connect(std::move(host), port,
                                   data.connection_timeout,
                                   data.max_retry_count, data.retry_delay)
      .and_then(this->with_ssl_connection_or_socket_select(
        use_ssl, [this, &cfg, &on_start](auto&& conn) {
          using conn_t = decltype(conn);
          return this->do_start_impl(cfg, std::forward<conn_t>(conn), on_start);
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
      .and_then(this->with_ssl_connection_or_socket(
        [this, &cfg, &on_start](auto&& conn) {
          using conn_t = decltype(conn);
          return this->do_start_impl(cfg, std::forward<conn_t>(conn), on_start);
        })

      );
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
