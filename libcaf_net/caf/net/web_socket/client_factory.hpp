// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/spsc_buffer.hpp"
#include "caf/detail/ws_flow_bridge.hpp"
#include "caf/disposable.hpp"
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
  : public ws_flow_bridge<Trait, net::web_socket::client> {
public:
  using super = ws_flow_bridge<Trait, net::web_socket::client>;

  // We consume the output type of the application.
  using pull_t = async::consumer_resource<typename Trait::input_type>;

  // We produce the input type of the application.
  using push_t = async::producer_resource<typename Trait::output_type>;

  ws_client_flow_bridge(async::execution_context_ptr loop, pull_t pull,
                        push_t push)
    : super(std::move(loop)), pull_(std::move(pull)), push_(std::move(push)) {
    // nop
  }

  static std::unique_ptr<ws_client_flow_bridge> make(net::multiplexer* mpx,
                                                     pull_t pull, push_t push) {
    return std::make_unique<ws_client_flow_bridge>(mpx, std::move(pull),
                                                   std::move(push));
  }

  error start(net::web_socket::lower_layer* down_ptr) override {
    super::down_ = down_ptr;
    return super::init(std::move(pull_), std::move(push_));
  }

private:
  pull_t pull_;
  push_t push_;
};

} // namespace caf::detail

namespace caf::net::web_socket {

template <class Trait>
class client_factory_config : public dsl::config_with_trait<Trait> {
public:
  using super = dsl::config_with_trait<Trait>;

  using super::super;

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

  using start_res_t = expected<disposable>;

  using config_type = dsl::client_config<client_factory_config<Trait>>;

  /// Starts a connection with the length-prefixing protocol.
  template <class OnStart>
  [[nodiscard]] expected<disposable> start(OnStart on_start) {
    using input_res_t = typename Trait::input_resource;
    using output_res_t = typename Trait::output_resource;
    static_assert(std::is_invocable_v<OnStart, input_res_t, output_res_t>);
    auto f = [this, &on_start](auto& cfg) {
      return this->do_start(cfg, on_start);
    };
    return visit(f, this->config());
  }

private:
  auto try_connect(const typename config_type::lazy& cfg,
                   const std::string& host, uint16_t port) {
    auto result = make_connected_tcp_stream_socket(host, port,
                                                   cfg.connection_timeout);
    if (result)
      return result;
    for (size_t i = 1; i <= cfg.max_retry_count; ++i) {
      std::this_thread::sleep_for(cfg.retry_delay);
      result = make_connected_tcp_stream_socket(host, port,
                                                cfg.connection_timeout);
      if (result)
        return result;
    }
    return result;
  }

  template <class Conn, class OnStart>
  start_res_t do_start_impl(config_type& cfg, net::web_socket::handshake& hs,
                            Conn conn, OnStart& on_start) {
    // s2a: socket-to-application (and a2s is the inverse).
    using input_t = typename Trait::input_type;
    using output_t = typename Trait::output_type;
    using transport_t = typename Conn::transport_type;
    using bridge_t = detail::ws_client_flow_bridge<Trait>;
    auto [s2a_pull, s2a_push] = async::make_spsc_buffer_resource<input_t>();
    auto [a2s_pull, a2s_push] = async::make_spsc_buffer_resource<output_t>();
    auto bridge = bridge_t::make(cfg.mpx, std::move(a2s_pull),
                                 std::move(s2a_push));
    auto bridge_ptr = bridge.get();
    auto impl = client::make(std::move(hs), std::move(bridge));
    auto fd = conn.fd();
    auto transport = transport_t::make(std::move(conn), std::move(impl));
    transport->active_policy().connect(fd);
    auto ptr = socket_manager::make(cfg.mpx, std::move(transport));
    bridge_ptr->self_ref(ptr->as_disposable());
    cfg.mpx->start(ptr);
    on_start(std::move(s2a_pull), std::move(a2s_push));
    return start_res_t{disposable{std::move(ptr)}};
  }

  template <class OnStart>
  start_res_t do_start(typename config_type::lazy& cfg, const std::string& host,
                       uint16_t port, std::string path, OnStart& on_start) {
    net::web_socket::handshake hs;
    hs.host(host);
    hs.endpoint(path);
    auto fd = try_connect(cfg, host, port);
    if (!fd) {
      cfg.call_on_error(fd.error());
      return start_res_t{std::move(fd.error())};
    }
    if (cfg.ctx) {
      auto conn = cfg.ctx->new_connection(*fd);
      if (conn)
        return do_start_impl(cfg, hs, std::move(*conn), on_start);
      cfg.call_on_error(conn.error());
      return start_res_t{std::move(conn.error())};
    }
    return do_start_impl(cfg, hs, *fd, on_start);
  }

  template <class OnStart>
  start_res_t do_start(typename config_type::lazy& cfg, OnStart& on_start) {
    if (auto* st = std::get_if<dsl::client_config_server_address>(&cfg.server))
      return do_start(cfg, st->host, st->port, "/", on_start);
    const auto& addr = std::get<uri>(cfg.server);
    if (addr.scheme() != "ws" && addr.scheme() != "wss") {
      return make_error(sec::invalid_argument, "URI must use ws or wss scheme");
    }
    if (addr.scheme() == "ws" && cfg.ctx) {
      return make_error(sec::logic_error, "found SSL config with scheme ws");
    }
    auto port = addr.authority().port;
    return do_start(cfg, std::string{addr.host_str()}, port == 0 ? 80 : port,
                    addr.path_query_fragment(), on_start);
  }

  template <class OnStart>
  start_res_t do_start(typename config_type::socket& cfg, OnStart&) {
    auto err = make_error(sec::logic_error, "not implemented yet");
    cfg.call_on_error(err);
    return start_res_t{std::move(err)};
    // return do_start_impl(cfg, cfg.take_fd(), on_start);
  }

  template <class OnStart>
  start_res_t do_start(typename config_type::conn& cfg, OnStart&) {
    auto err = make_error(sec::logic_error, "not implemented yet");
    cfg.call_on_error(err);
    return start_res_t{std::move(err)};
    // return do_start_impl(cfg, std::move(cfg.state), on_start);
  }

  template <class OnStart>
  start_res_t do_start(typename config_type::fail& cfg, OnStart&) {
    cfg.call_on_error(cfg.err);
    return start_res_t{std::move(cfg.err)};
  }
};

} // namespace caf::net::web_socket
