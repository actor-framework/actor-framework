// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/dsl/server_factory_base.hpp"
#include "caf/net/http/server.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/octet_stream/transport.hpp"
#include "caf/net/ssl/transport.hpp"
#include "caf/net/tcp_accept_socket.hpp"
#include "caf/net/web_socket/acceptor.hpp"
#include "caf/net/web_socket/config.hpp"
#include "caf/net/web_socket/server.hpp"

#include "caf/async/blocking_producer.hpp"
#include "caf/callback.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/accept_handler.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/connection_factory.hpp"
#include "caf/detail/ws_flow_bridge.hpp"
#include "caf/fwd.hpp"

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

  using on_request_cb_type = shared_callback_ptr<void(ws_acceptor_t&)>;

  using accept_event = typename Trait::template accept_event<Ts...>;

  using producer_type = async::blocking_producer<accept_event>;

  using acceptor_impl_t = net::web_socket::acceptor_impl<Trait, Ts...>;

  using ws_res_type = typename acceptor_impl_t::ws_res_type;

  // Note: this is shared with the connection factory. In general, this is
  //       *unsafe*. However, we exploit the fact that there is currently only
  //       one thread running in the multiplexer (which makes this safe).
  using shared_producer_type = std::shared_ptr<producer_type>;

  ws_server_flow_bridge(on_request_cb_type on_request,
                        shared_producer_type producer)
    : on_request_(std::move(on_request)), producer_(std::move(producer)) {
    // nop
  }

  static auto make(on_request_cb_type on_request,
                   shared_producer_type producer) {
    return std::make_unique<ws_server_flow_bridge>(std::move(on_request),
                                                   std::move(producer));
  }

  error start(net::web_socket::lower_layer* down_ptr) override {
    CAF_ASSERT(down_ptr != nullptr);
    super::down_ = down_ptr;
    if (!producer_->push(app_event)) {
      return error{sec::runtime_error,
                   "WebSocket connection dropped: client canceled"};
    }
    auto& [pull, push] = ws_resources;
    return super::init(&down_ptr->mpx(), std::move(pull), std::move(push));
  }

  error accept(const net::http::request_header& hdr) override {
    net::web_socket::acceptor_impl<Trait, Ts...> acc{hdr};
    (*on_request_)(acc);
    if (acc.accepted()) {
      app_event = std::move(acc.app_event);
      ws_resources = std::move(acc.ws_resources);
      return {};
    }
    return std::move(acc) //
      .reject_reason()
      .or_else(sec::runtime_error, "WebSocket request rejected without reason");
  }

private:
  on_request_cb_type on_request_;
  shared_producer_type producer_;
  accept_event app_event;
  ws_res_type ws_resources;
};

/// Specializes @ref connection_factory for flows over the WebSocket protocol.
template <class Transport, class Trait, class... Ts>
class ws_flow_conn_factory
  : public connection_factory<typename Transport::connection_handle> {
public:
  using ws_acceptor_t = net::web_socket::acceptor<Ts...>;

  using on_request_cb_type = shared_callback_ptr<void(ws_acceptor_t&)>;

  using accept_event = typename Trait::template accept_event<Ts...>;

  using producer_type = async::blocking_producer<accept_event>;

  using shared_producer_type = std::shared_ptr<producer_type>;

  using connection_handle = typename Transport::connection_handle;

  ws_flow_conn_factory(on_request_cb_type on_request,
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
    auto app = bridge_t::make(on_request_, producer_);
    auto app_ptr = app.get();
    auto ws = net::web_socket::server::make(std::move(app));
    auto transport = Transport::make(std::move(conn), std::move(ws));
    transport->max_consecutive_reads(max_consecutive_reads_);
    transport->active_policy().accept();
    auto mgr = net::socket_manager::make(mpx, std::move(transport));
    app_ptr->self_ref(mgr->as_disposable());
    return mgr;
  }

private:
  on_request_cb_type on_request_;
  shared_producer_type producer_;
  size_t max_consecutive_reads_;
};

/// Specializes @ref connection_factory for custom upper layer implementations.
template <class Transport, class MakeApp>
class ws_simple_conn_factory
  : public connection_factory<typename Transport::connection_handle> {
public:
  using connection_handle = typename Transport::connection_handle;

  ws_simple_conn_factory(MakeApp app_factory, size_t max_consecutive_reads)
    : max_consecutive_reads_(max_consecutive_reads),
      app_factory_(std::move(app_factory)) {
    // nop
  }

  net::socket_manager_ptr make(net::multiplexer* mpx,
                               connection_handle conn) override {
    auto app = app_factory_();
    auto ws = net::web_socket::server::make(std::move(app));
    auto transport = Transport::make(std::move(conn), std::move(ws));
    transport->max_consecutive_reads(max_consecutive_reads_);
    transport->active_policy().accept();
    return net::socket_manager::make(mpx, std::move(transport));
  }

private:
  size_t max_consecutive_reads_;
  MakeApp app_factory_;
};

} // namespace caf::detail

namespace caf::net::web_socket {

/// Factory type for the `with(...).accept(...).start(...)` DSL.
template <class Trait, class... Ts>
class server_factory : public dsl::server_factory_base<server_config<Trait>,
                                                       server_factory<Trait>> {
public:
  using super
    = dsl::server_factory_base<server_config<Trait>, server_factory<Trait>>;

  using config_type = typename super::config_type;

  using on_request_cb_type = shared_callback_ptr<void(acceptor<Ts...>&)>;

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
    using token_t
      = std::conditional_t<std::is_invocable_v<OnStart, acceptor_resource>,
                           flow_impl_token, custom_impl_token>;
    if constexpr (std::is_same_v<token_t, custom_impl_token>) {
      using server_t = web_socket::upper_layer::server;
      using server_ptr_t = std::unique_ptr<server_t>;
      static_assert(std::is_invocable_r_v<server_ptr_t, OnStart>,
                    "OnStart must have signature 'void(acceptor_resource)' or "
                    "unique_ptr<web_socket::upper_layer::server>()'");
    }
    auto& cfg = super::config();
    return cfg.visit([this, &cfg, &on_start](auto& data) {
      return this->do_start(cfg, data, on_start, token_t{})
        .or_else([&cfg](const error& err) { cfg.call_on_error(err); });
    });
  }

private:
  struct flow_impl_token {};

  struct custom_impl_token {};

  template <class Acceptor, class OnStart>
  expected<disposable> do_start_impl(config_type& cfg, Acceptor acc,
                                     OnStart& on_start, flow_impl_token) {
    using transport_t = typename Acceptor::transport_type;
    using factory_t = detail::ws_flow_conn_factory<transport_t, Trait, Ts...>;
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

  template <class Acceptor, class MakeApp>
  expected<disposable> do_start_impl(config_type& cfg, Acceptor acc,
                                     MakeApp& make_app, custom_impl_token) {
    using transport_t = typename Acceptor::transport_type;
    using factory_t = detail::ws_simple_conn_factory<transport_t, MakeApp>;
    using impl_t = detail::accept_handler<Acceptor>;
    auto factory = std::make_unique<factory_t>(std::move(make_app),
                                               cfg.max_consecutive_reads);
    auto impl = impl_t::make(std::move(acc), std::move(factory),
                             cfg.max_connections);
    auto impl_ptr = impl.get();
    auto ptr = net::socket_manager::make(cfg.mpx, std::move(impl));
    impl_ptr->self_ref(ptr->as_disposable());
    cfg.mpx->start(ptr);
    return expected<disposable>{disposable{std::move(ptr)}};
  }

  template <class OnStart, class Token>
  expected<disposable> do_start(config_type& cfg,
                                dsl::server_config::socket& data,
                                OnStart& on_start, Token) {
    return checked_socket(data.take_fd())
      .and_then(
        this->with_ssl_acceptor_or_socket([this, &cfg, &on_start](auto&& acc) {
          using acc_t = decltype(acc);
          return this->do_start_impl(cfg, std::forward<acc_t>(acc), on_start,
                                     Token{});
        }));
  }

  template <class OnStart, class Token>
  expected<disposable> do_start(config_type& cfg,
                                dsl::server_config::lazy& data,
                                OnStart& on_start, Token) {
    return make_tcp_accept_socket(data.port, data.bind_address, data.reuse_addr)
      .and_then(
        this->with_ssl_acceptor_or_socket([this, &cfg, &on_start](auto&& acc) {
          using acc_t = decltype(acc);
          return this->do_start_impl(cfg, std::forward<acc_t>(acc), on_start,
                                     Token{});
        }));
  }

  template <class OnStart, class Token>
  expected<disposable> do_start(config_type&, error& err, OnStart&, Token) {
    return expected<disposable>{std::move(err)};
  }

  on_request_cb_type on_request_;
};

} // namespace caf::net::web_socket
