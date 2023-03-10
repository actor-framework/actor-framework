// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/defaults.hpp"
#include "caf/detail/accept_handler.hpp"
#include "caf/detail/binary_flow_bridge.hpp"
#include "caf/detail/connection_factory.hpp"
#include "caf/detail/flow_connector.hpp"
#include "caf/detail/shared_ssl_acceptor.hpp"
#include "caf/fwd.hpp"
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
#include <variant>

namespace caf::detail {

/// Specializes @ref connection_factory for the length-prefixing protocol.
template <class Trait, class Transport>
class lp_connection_factory
  : public connection_factory<typename Transport::connection_handle> {
public:
  using connection_handle = typename Transport::connection_handle;

  using connector_ptr = flow_connector_ptr<Trait>;

  explicit lp_connection_factory(connector_ptr connector)
    : connector_(std::move(connector)) {
    // nop
  }

  net::socket_manager_ptr make(net::multiplexer* mpx,
                               connection_handle conn) override {
    auto bridge = binary_flow_bridge<Trait>::make(mpx, connector_);
    auto bridge_ptr = bridge.get();
    auto impl = net::lp::framing::make(std::move(bridge));
    auto fd = conn.fd();
    auto transport = Transport::make(std::move(conn), std::move(impl));
    transport->active_policy().accept(fd);
    auto mgr = net::socket_manager::make(mpx, std::move(transport));
    bridge_ptr->self_ref(mgr->as_disposable());
    return mgr;
  }

private:
  connector_ptr connector_;
};

} // namespace caf::detail

namespace caf::net::lp {

/// Factory type for the `with(...).accept(...).start(...)` DSL.
template <class Trait>
class server_factory
  : public dsl::server_factory_base<Trait, server_factory<Trait>> {
public:
  using super = dsl::server_factory_base<Trait, server_factory<Trait>>;

  using super::super;

  using start_res_t = expected<disposable>;

  /// Starts a server that accepts incoming connections with the
  /// length-prefixing protocol.
  template <class OnStart>
  start_res_t start(OnStart on_start) {
    using acceptor_resource = typename Trait::acceptor_resource;
    static_assert(std::is_invocable_v<OnStart, acceptor_resource>);
    auto f = [this, &on_start](auto& cfg) {
      return this->do_start(cfg, on_start);
    };
    return visit(f, this->config());
  }

private:
  template <class Factory, class AcceptHandler, class Acceptor, class OnStart>
  start_res_t do_start_impl(dsl::server_config<Trait>& cfg, Acceptor acc,
                            OnStart& on_start) {
    using accept_event = typename Trait::accept_event;
    using connector_t = detail::flow_connector<Trait>;
    auto [pull, push] = async::make_spsc_buffer_resource<accept_event>();
    auto serv = connector_t::make_basic_server(push.try_open());
    auto factory = std::make_unique<Factory>(std::move(serv));
    auto impl = AcceptHandler::make(std::move(acc), std::move(factory),
                                    cfg.max_connections);
    auto impl_ptr = impl.get();
    auto ptr = net::socket_manager::make(cfg.mpx, std::move(impl));
    impl_ptr->self_ref(ptr->as_disposable());
    cfg.mpx->start(ptr);
    on_start(std::move(pull));
    return start_res_t{disposable{std::move(ptr)}};
  }

  template <class OnStart>
  start_res_t do_start(dsl::server_config<Trait>& cfg, tcp_accept_socket fd,
                       OnStart& on_start) {
    if (!cfg.ctx) {
      using factory_t = detail::lp_connection_factory<Trait, stream_transport>;
      using impl_t = detail::accept_handler<tcp_accept_socket, stream_socket>;
      return do_start_impl<factory_t, impl_t>(cfg, fd, on_start);
    }
    using factory_t = detail::lp_connection_factory<Trait, ssl::transport>;
    using acc_t = detail::shared_ssl_acceptor;
    using impl_t = detail::accept_handler<acc_t, ssl::connection>;
    return do_start_impl<factory_t, impl_t>(cfg, acc_t{fd, cfg.ctx}, on_start);
  }

  template <class OnStart>
  start_res_t
  do_start(typename dsl::server_config<Trait>::socket& cfg, OnStart& on_start) {
    if (cfg.fd == invalid_socket) {
      auto err = make_error(
        sec::runtime_error,
        "server factory cannot create a server on an invalid socket");
      cfg.call_on_error(err);
      return start_res_t{std::move(err)};
    }
    return do_start(cfg, cfg.take_fd(), on_start);
  }

  template <class OnStart>
  start_res_t
  do_start(typename dsl::server_config<Trait>::lazy& cfg, OnStart& on_start) {
    auto fd = make_tcp_accept_socket(cfg.port, cfg.bind_address,
                                     cfg.reuse_addr);
    if (!fd) {
      cfg.call_on_error(fd.error());
      return start_res_t{std::move(fd.error())};
    }
    return do_start(cfg, *fd, on_start);
  }

  template <class OnStart>
  start_res_t do_start(dsl::fail_server_config<Trait>& cfg, OnStart&) {
    cfg.call_on_error(cfg.err);
    return start_res_t{std::move(cfg.err)};
  }
};

} // namespace caf::net::lp
