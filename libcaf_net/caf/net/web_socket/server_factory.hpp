// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/blocking_producer.hpp"
#include "caf/callback.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/accept_handler.hpp"
#include "caf/detail/connection_factory.hpp"
#include "caf/detail/ws_flow_bridge.hpp"
#include "caf/fwd.hpp"
#include "caf/net/dsl/server_factory_base.hpp"
#include "caf/net/http/request_header.hpp"
#include "caf/net/http/server.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/octet_stream/transport.hpp"
#include "caf/net/ssl/transport.hpp"
#include "caf/net/tcp_accept_socket.hpp"
#include "caf/net/web_socket/server.hpp"

#include <cstdint>
#include <functional>
#include <variant>

namespace caf::detail {

/// Specializes the WebSocket flow bridge for the server side.
template <class Trait, class... Ts>
class ws_server_flow_bridge
  : public ws_flow_bridge<Trait, net::web_socket::upper_layer::server> {
public:
  using super = ws_flow_bridge<Trait, net::web_socket::upper_layer::server>;

  using ws_acceptor_t = net::web_socket::acceptor<Ts...>;

  using on_request_cb_type
    = shared_callback_ptr<void(ws_acceptor_t&, //
                               const net::http::request_header&)>;

  using accept_event = typename Trait::template accept_event<Ts...>;

  using producer_type = async::blocking_producer<accept_event>;

  // Note: this is shared with the connection factory. In general, this is
  //       *unsafe*. However, we exploit the fact that there is currently only
  //       one thread running in the multiplexer (which makes this safe).
  using shared_producer_type = std::shared_ptr<producer_type>;

  ws_server_flow_bridge(async::execution_context_ptr loop,
                        on_request_cb_type on_request,
                        shared_producer_type producer)
    : super(std::move(loop)),
      on_request_(std::move(on_request)),
      producer_(std::move(producer)) {
    // nop
  }

  static auto make(net::multiplexer* mpx, on_request_cb_type on_request,
                   shared_producer_type producer) {
    return std::make_unique<ws_server_flow_bridge>(mpx, std::move(on_request),
                                                   std::move(producer));
  }

  error start(net::web_socket::lower_layer* down_ptr,
              const net::http::request_header& hdr) override {
    CAF_ASSERT(down_ptr != nullptr);
    super::down_ = down_ptr;
    net::web_socket::acceptor_impl<Trait, Ts...> acc;
    (*on_request_)(acc, hdr);
    if (!acc.accepted()) {
      return std::move(acc) //
        .reject_reason()
        .or_else(sec::runtime_error,
                 "WebSocket request rejected without reason");
    }
    if (!producer_->push(acc.app_event)) {
      return make_error(sec::runtime_error,
                        "WebSocket connection dropped: client canceled");
    }
    auto& [pull, push] = acc.ws_resources;
    return super::init(std::move(pull), std::move(push));
  }

private:
  on_request_cb_type on_request_;
  shared_producer_type producer_;
};

/// Specializes @ref connection_factory for the WebSocket protocol.
template <class Transport, class Trait, class... Ts>
class ws_connection_factory
  : public connection_factory<typename Transport::connection_handle> {
public:
  using ws_acceptor_t = net::web_socket::acceptor<Ts...>;

  using on_request_cb_type
    = shared_callback_ptr<void(ws_acceptor_t&, //
                               const net::http::request_header&)>;

  using accept_event = typename Trait::template accept_event<Ts...>;

  using producer_type = async::blocking_producer<accept_event>;

  using shared_producer_type = std::shared_ptr<producer_type>;

  using connection_handle = typename Transport::connection_handle;

  ws_connection_factory(on_request_cb_type on_request,
                        shared_producer_type producer,
                        size_t max_consecutive_reads)
    : on_request_(std::move(on_request)),
      producer_(std::move(producer)),
      max_consecutive_reads_(max_consecutive_reads) {
    // nop
  }

  net::socket_manager_ptr make(net::multiplexer* mpx,
                               connection_handle conn) override {
    if (producer_->canceled()) {
      // TODO: stop the caller?
      return nullptr;
    }
    using bridge_t = ws_server_flow_bridge<Trait, Ts...>;
    auto app = bridge_t::make(mpx, on_request_, producer_);
    auto app_ptr = app.get();
    auto ws = net::web_socket::server::make(std::move(app));
    auto fd = conn.fd();
    auto transport = Transport::make(std::move(conn), std::move(ws));
    transport->max_consecutive_reads(max_consecutive_reads_);
    transport->active_policy().accept(fd);
    auto mgr = net::socket_manager::make(mpx, std::move(transport));
    app_ptr->self_ref(mgr->as_disposable());
    return mgr;
  }

private:
  on_request_cb_type on_request_;
  shared_producer_type producer_;
  size_t max_consecutive_reads_;
};

} // namespace caf::detail

namespace caf::net::web_socket {

/// Factory type for the `with(...).accept(...).start(...)` DSL.
template <class Trait, class... Ts>
class server_factory
  : public dsl::server_factory_base<dsl::config_with_trait<Trait>,
                                    server_factory<Trait>> {
public:
  using super = dsl::server_factory_base<dsl::config_with_trait<Trait>,
                                         server_factory<Trait>>;

  using config_type = typename super::config_type;

  using on_request_cb_type
    = shared_callback_ptr<void(acceptor<Ts...>&, const http::request_header&)>;

  server_factory(intrusive_ptr<config_type> cfg, on_request_cb_type on_request)
    : super(std::move(cfg)), on_request_(std::move(on_request)) {
    // nop
  }

  /// The input type of the application, i.e., what that flows from the
  /// WebSocket to the application layer.
  using input_type = typename Trait::input_type;

  /// The output type of the application, i.e., what flows from the application
  /// layer to the WebSocket.
  using output_type = typename Trait::output_type;

  /// A resource for consuming input_type elements.
  using input_resource = async::consumer_resource<input_type>;

  /// A resource for producing output_type elements.
  using output_resource = async::producer_resource<output_type>;

  /// An accept event from the server to transmit read and write handles.
  using accept_event = cow_tuple<input_resource, output_resource, Ts...>;

  /// A resource for consuming accept events.
  using acceptor_resource = async::consumer_resource<accept_event>;

  /// Starts a server that accepts incoming connections with the WebSocket
  /// protocol.
  template <class OnStart>
  expected<disposable> start(OnStart on_start) {
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
    using factory_t = detail::ws_connection_factory<transport_t, Trait, Ts...>;
    using impl_t = detail::accept_handler<Acceptor>;
    using producer_t = async::blocking_producer<accept_event>;
    auto [pull, push] = async::make_spsc_buffer_resource<accept_event>();
    auto producer = std::make_shared<producer_t>(producer_t{push.try_open()});
    auto factory = std::make_unique<factory_t>(on_request_, std::move(producer),
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

  on_request_cb_type on_request_;
};

} // namespace caf::net::web_socket
