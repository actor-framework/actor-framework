// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/blocking_producer.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/accept_handler.hpp"
#include "caf/detail/binary_flow_bridge.hpp"
#include "caf/detail/connection_factory.hpp"
#include "caf/detail/shared_ssl_acceptor.hpp"
#include "caf/fwd.hpp"
#include "caf/net/checked_socket.hpp"
#include "caf/net/dsl/server_factory_base.hpp"
#include "caf/net/http/server.hpp"
#include "caf/net/lp/framing.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/ssl/transport.hpp"
#include "caf/net/stream_transport.hpp"
#include "caf/net/tcp_accept_socket.hpp"
#include "caf/none.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <variant>

namespace caf::detail {

/// Specializes the length-prefix flow bridge for the server side.
template <class Trait>
class lp_server_flow_bridge : public binary_flow_bridge<Trait> {
public:
  using super = binary_flow_bridge<Trait>;

  using input_type = typename Trait::input_type;

  using output_type = typename Trait::output_type;

  using accept_event = typename Trait::accept_event;

  using producer_type = async::blocking_producer<accept_event>;

  // Note: this is shared with the connection factory. In general, this is
  //       *unsafe*. However, we exploit the fact that there is currently only
  //       one thread running in the multiplexer (which makes this safe).
  using shared_producer_type = std::shared_ptr<producer_type>;

  lp_server_flow_bridge(async::execution_context_ptr loop,
                        shared_producer_type producer)
    : super(std::move(loop)), producer_(std::move(producer)) {
    // nop
  }

  static auto make(net::multiplexer* mpx, shared_producer_type producer) {
    return std::make_unique<lp_server_flow_bridge>(mpx, std::move(producer));
  }

  error start(net::binary::lower_layer* down_ptr) override {
    CAF_ASSERT(down_ptr != nullptr);
    super::down_ = down_ptr;
    auto [app_pull, lp_push] = async::make_spsc_buffer_resource<input_type>();
    auto [lp_pull, app_push] = async::make_spsc_buffer_resource<output_type>();
    auto event = accept_event{std::move(app_pull), std::move(app_push)};
    if (!producer_->push(event)) {
      return make_error(sec::runtime_error,
                        "Length-prefixed connection dropped: client canceled");
    }
    return super::init(std::move(lp_pull), std::move(lp_push));
  }

private:
  shared_producer_type producer_;
};

/// Specializes @ref connection_factory for the length-prefixing protocol.
template <class Transport, class Trait>
class lp_connection_factory
  : public connection_factory<typename Transport::connection_handle> {
public:
  using accept_event = typename Trait::accept_event;

  using producer_type = async::blocking_producer<accept_event>;

  using shared_producer_type = std::shared_ptr<producer_type>;

  using connection_handle = typename Transport::connection_handle;

  lp_connection_factory(producer_type producer, size_t max_consecutive_reads)
    : producer_(std::make_shared<producer_type>(std::move(producer))),
      max_consecutive_reads_(max_consecutive_reads) {
    // nop
  }

  net::socket_manager_ptr make(net::multiplexer* mpx,
                               connection_handle conn) override {
    using bridge_t = lp_server_flow_bridge<Trait>;
    auto bridge = bridge_t::make(mpx, producer_);
    auto bridge_ptr = bridge.get();
    auto impl = net::lp::framing::make(std::move(bridge));
    auto fd = conn.fd();
    auto transport = Transport::make(std::move(conn), std::move(impl));
    transport->max_consecutive_reads(max_consecutive_reads_);
    transport->active_policy().accept(fd);
    auto mgr = net::socket_manager::make(mpx, std::move(transport));
    bridge_ptr->self_ref(mgr->as_disposable());
    return mgr;
  }

private:
  shared_producer_type producer_;
  size_t max_consecutive_reads_;
};

} // namespace caf::detail

namespace caf::net::lp {

/// Factory type for the `with(...).accept(...).start(...)` DSL.
template <class Trait>
class server_factory
  : public dsl::server_factory_base<dsl::config_with_trait<Trait>,
                                    server_factory<Trait>> {
public:
  using super = dsl::server_factory_base<dsl::config_with_trait<Trait>,
                                         server_factory<Trait>>;

  using config_type = typename super::config_type;

  using super::super;

  /// Starts a server that accepts incoming connections with the
  /// length-prefixing protocol.
  template <class OnStart>
  [[nodiscard]] expected<disposable> start(OnStart on_start) {
    using acceptor_resource = typename Trait::acceptor_resource;
    static_assert(std::is_invocable_v<OnStart, acceptor_resource>);
    auto& cfg = super::config();
    return cfg.visit([this, &cfg, &on_start](auto& data) {
      return this->do_start(cfg, data, on_start)
        .or_else([&cfg](const error& err) { cfg.call_on_error(err); });
    });
  }

private:
  template <class Acceptor, class OnStart>
  expected<disposable>
  do_start_impl(config_type& cfg, Acceptor acc, OnStart& on_start) {
    using transport_t = typename Acceptor::transport_type;
    using factory_t = detail::lp_connection_factory<transport_t, Trait>;
    using impl_t = detail::accept_handler<Acceptor>;
    using accept_event = typename Trait::accept_event;
    auto [pull, push] = async::make_spsc_buffer_resource<accept_event>();
    auto producer = async::make_blocking_producer(push.try_open());
    auto factory = std::make_unique<factory_t>(std::move(producer),
                                               cfg.max_consecutive_reads);
    auto impl = impl_t::make(std::move(acc), std::move(factory),
                             cfg.max_connections);
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

} // namespace caf::net::lp
