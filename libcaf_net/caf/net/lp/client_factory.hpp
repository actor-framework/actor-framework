// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/checked_socket.hpp"
#include "caf/net/dsl/client_factory_base.hpp"
#include "caf/net/lp/config.hpp"
#include "caf/net/lp/framing.hpp"
#include "caf/net/ssl/connection.hpp"
#include "caf/net/tcp_stream_socket.hpp"

#include "caf/async/spsc_buffer.hpp"
#include "caf/detail/lp_flow_bridge.hpp"
#include "caf/disposable.hpp"
#include "caf/timespan.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <variant>

namespace caf::detail {

/// Specializes the WebSocket flow bridge for the server side.
template <class Trait>
class lp_client_flow_bridge : public lp_flow_bridge<Trait> {
public:
  using super = lp_flow_bridge<Trait>;

  // We consume the output type of the application.
  using pull_t = async::consumer_resource<typename Trait::output_type>;

  // We produce the input type of the application.
  using push_t = async::producer_resource<typename Trait::input_type>;

  lp_client_flow_bridge(pull_t pull, push_t push)
    : pull_(std::move(pull)), push_(std::move(push)) {
    // nop
  }

  static std::unique_ptr<lp_client_flow_bridge> make(pull_t pull, push_t push) {
    return std::make_unique<lp_client_flow_bridge>(std::move(pull),
                                                   std::move(push));
  }

  void abort(const error& err) override {
    super::abort(err);
    if (push_)
      push_.abort(err);
  }

  error start(net::lp::lower_layer* down_ptr) override {
    super::down_ = down_ptr;
    return super::init(&down_ptr->mpx(), std::move(pull_), std::move(push_));
  }

private:
  pull_t pull_;
  push_t push_;
};

} // namespace caf::detail

namespace caf::net::lp {

/// Factory for the `with(...).connect(...).start(...)` DSL.
template <class Trait>
class client_factory : public dsl::client_factory_base<client_config<Trait>,
                                                       client_factory<Trait>> {
public:
  using super
    = dsl::client_factory_base<client_config<Trait>, client_factory<Trait>>;

  using config_type = typename super::config_type;

  using super::super;

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
    using bridge_t = detail::lp_client_flow_bridge<Trait>;
    using transport_t = typename Conn::transport_type;
    auto [s2a_pull, s2a_push] = async::make_spsc_buffer_resource<input_t>();
    auto [a2s_pull, a2s_push] = async::make_spsc_buffer_resource<output_t>();
    auto bridge = bridge_t::make(std::move(a2s_pull), std::move(s2a_push));
    auto bridge_ptr = bridge.get();
    auto impl = framing::make(std::move(bridge));
    auto transport = transport_t::make(std::move(conn), std::move(impl));
    transport->active_policy().connect();
    auto ptr = socket_manager::make(cfg.mpx, std::move(transport));
    bridge_ptr->self_ref(ptr->as_disposable());
    cfg.mpx->start(ptr);
    on_start(std::move(s2a_pull), std::move(a2s_push));
    return expected<disposable>{disposable{std::move(ptr)}};
  }

  template <class OnStart>
  expected<disposable> do_start(config_type& cfg,
                                dsl::client_config::lazy& data,
                                OnStart& on_start) {
    if (std::holds_alternative<uri>(data.server)) {
      auto err = make_error(sec::invalid_argument,
                            "length-prefix factories do not accept URIs");
      return do_start(cfg, err, on_start);
    }
    auto& addr = std::get<dsl::server_address>(data.server);
    return detail::tcp_try_connect(std::move(addr.host), addr.port,
                                   data.connection_timeout,
                                   data.max_retry_count, data.retry_delay)
      .and_then(data.connection_with_ctx([this, &cfg, &on_start](auto& conn) {
        return this->do_start_impl(cfg, std::move(conn), on_start);
      }));
  }

  template <class OnStart>
  expected<disposable> do_start(config_type& cfg,
                                dsl::client_config::socket& data,
                                OnStart& on_start) {
    return checked_socket(data.take_fd())
      .and_then(data.connection_with_ctx([this, &cfg, &on_start](auto& conn) {
        return this->do_start_impl(cfg, std::move(conn), on_start);
      }));
  }

  template <class OnStart>
  expected<disposable> do_start(config_type& cfg,
                                dsl::client_config::conn& data,
                                OnStart& on_start) {
    return do_start_impl(cfg, std::move(data.state), on_start);
  }

  template <class OnStart>
  expected<disposable> do_start(config_type& cfg, error& err, OnStart&) {
    cfg.call_on_error(err);
    return expected<disposable>{std::move(err)};
  }
};

} // namespace caf::net::lp
