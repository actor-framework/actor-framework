// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/spsc_buffer.hpp"
#include "caf/detail/binary_flow_bridge.hpp"
#include "caf/detail/flow_connector.hpp"
#include "caf/disposable.hpp"
#include "caf/net/dsl/client_factory_base.hpp"
#include "caf/net/lp/framing.hpp"
#include "caf/net/ssl/connection.hpp"
#include "caf/net/tcp_stream_socket.hpp"
#include "caf/timespan.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <variant>

namespace caf::net::lp {

template <class>
class with_t;

/// Factory for the `with(...).connect(...).start(...)` DSL.
template <class Trait>
class client_factory
  : public dsl::client_factory_base<Trait, client_factory<Trait>> {
public:
  using super = dsl::client_factory_base<Trait, client_factory<Trait>>;

  using super::super;

  using start_res_t = expected<disposable>;

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
  auto try_connect(const dsl::lazy_client_config<Trait>& cfg,
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
  start_res_t
  do_start_impl(dsl::client_config<Trait>& cfg, Conn conn, OnStart& on_start) {
    // s2a: socket-to-application (and a2s is the inverse).
    using input_t = typename Trait::input_type;
    using output_t = typename Trait::output_type;
    using transport_t = typename Conn::transport_type;
    auto [s2a_pull, s2a_push] = async::make_spsc_buffer_resource<input_t>();
    auto [a2s_pull, a2s_push] = async::make_spsc_buffer_resource<output_t>();
    auto fc = detail::flow_connector<Trait>::make_trivial(std::move(a2s_pull),
                                                          std::move(s2a_push));
    auto bridge = detail::binary_flow_bridge<Trait>::make(cfg.mpx,
                                                          std::move(fc));
    auto bridge_ptr = bridge.get();
    auto impl = framing::make(std::move(bridge));
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
  start_res_t do_start(dsl::lazy_client_config<Trait>& cfg,
                       const std::string& host, uint16_t port,
                       OnStart& on_start) {
    auto fd = try_connect(cfg, host, port);
    if (fd) {
      if (cfg.ctx) {
        auto conn = cfg.ctx->new_connection(*fd);
        if (conn)
          return do_start_impl(cfg, std::move(*conn), on_start);
        cfg.call_on_error(conn.error());
        return start_res_t{std::move(conn.error())};
      }
      return do_start_impl(cfg, *fd, on_start);
    }
    cfg.call_on_error(fd.error());
    return start_res_t{std::move(fd.error())};
  }

  template <class OnStart>
  start_res_t do_start(dsl::lazy_client_config<Trait>& cfg, OnStart& on_start) {
    if (auto* st = std::get_if<dsl::client_config_server_address>(&cfg.server))
      return do_start(cfg, st->host, st->port, on_start);
    auto err = make_error(sec::invalid_argument,
                          "length-prefix factories do not accept URIs");
    cfg.call_on_error(err);
    return start_res_t{std::move(err)};
  }

  template <class OnStart>
  start_res_t
  do_start(dsl::socket_client_config<Trait>& cfg, OnStart& on_start) {
    return do_start_impl(cfg, cfg.take_fd(), on_start);
  }

  template <class OnStart>
  start_res_t do_start(dsl::conn_client_config<Trait>& cfg, OnStart& on_start) {
    return do_start_impl(cfg, std::move(cfg.state), on_start);
  }

  template <class OnStart>
  start_res_t do_start(dsl::fail_client_config<Trait>& cfg, OnStart&) {
    cfg.call_on_error(cfg.err);
    return start_res_t{std::move(cfg.err)};
  }
};

} // namespace caf::net::lp
