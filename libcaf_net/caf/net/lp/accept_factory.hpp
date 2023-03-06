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
#include "caf/net/http/server.hpp"
#include "caf/net/lp/framing.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/prometheus/accept_factory.hpp"
#include "caf/net/prometheus/server.hpp"
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

template <class>
class with_t;

/// Factory for the `with(...).accept(...).start(...)` DSL.
template <class Trait>
class accept_factory {
public:
  friend class with_t<Trait>;

  accept_factory(accept_factory&&) = default;

  accept_factory(const accept_factory&) = delete;

  accept_factory& operator=(accept_factory&&) noexcept = default;

  accept_factory& operator=(const accept_factory&) noexcept = delete;

  ~accept_factory() {
    if (auto* fd = std::get_if<tcp_accept_socket>(&state_))
      close(*fd);
  }

  /// Configures how many concurrent connections we are allowing.
  accept_factory& max_connections(size_t value) {
    max_connections_ = value;
    return *this;
  }

  /// Sets the callback for errors.
  template <class F>
  accept_factory& do_on_error(F callback) {
    do_on_error_ = std::move(callback);
    return *this;
  }

  /// Starts a server that accepts incoming connections with the
  /// length-prefixing protocol.
  template <class OnStart>
  disposable start(OnStart on_start) {
    using acceptor_resource = typename Trait::acceptor_resource;
    static_assert(std::is_invocable_v<OnStart, acceptor_resource>);
    switch (state_.index()) {
      case 1: {
        auto& cfg = std::get<1>(state_);
        auto fd = make_tcp_accept_socket(cfg.port, cfg.address, cfg.reuse_addr);
        if (fd)
          return do_start(*fd, on_start);
        if (do_on_error_)
          do_on_error_(fd.error());
        return {};
      }
      case 2: {
        // Pass ownership of the socket to the accept handler.
        auto fd = std::get<2>(state_);
        state_ = none;
        return do_start(fd, on_start);
      }
      default:
        return {};
    }
  }

private:
  struct config {
    uint16_t port;
    std::string address;
    bool reuse_addr;
  };

  explicit accept_factory(multiplexer* mpx) : mpx_(mpx) {
    // nop
  }

  template <class Factory, class AcceptHandler, class Acceptor, class OnStart>
  disposable do_start_impl(Acceptor&& acc, OnStart& on_start) {
    using accept_event = typename Trait::accept_event;
    using connector_t = detail::flow_connector<Trait>;
    auto [pull, push] = async::make_spsc_buffer_resource<accept_event>();
    auto serv = connector_t::make_basic_server(push.try_open());
    auto factory = std::make_unique<Factory>(std::move(serv));
    auto impl = AcceptHandler::make(std::move(acc), std::move(factory),
                                    max_connections_);
    auto impl_ptr = impl.get();
    auto ptr = net::socket_manager::make(mpx_, std::move(impl));
    impl_ptr->self_ref(ptr->as_disposable());
    mpx_->start(ptr);
    on_start(std::move(pull));
    return disposable{std::move(ptr)};
  }

  template <class OnStart>
  disposable do_start(tcp_accept_socket fd, OnStart& on_start) {
    if (!ctx_) {
      using factory_t = detail::lp_connection_factory<Trait, stream_transport>;
      using impl_t = detail::accept_handler<tcp_accept_socket, stream_socket>;
      return do_start_impl<factory_t, impl_t>(fd, on_start);
    }
    using factory_t = detail::lp_connection_factory<Trait, ssl::transport>;
    using acc_t = detail::shared_ssl_acceptor;
    using impl_t = detail::accept_handler<acc_t, ssl::connection>;
    return do_start_impl<factory_t, impl_t>(acc_t{fd, ctx_}, on_start);
  }

  void set_ssl(ssl::context ctx) {
    ctx_ = std::make_shared<ssl::context>(std::move(ctx));
  }

  void init(uint16_t port, std::string address, bool reuse_addr) {
    state_ = config{port, std::move(address), reuse_addr};
  }

  void init(tcp_accept_socket fd) {
    state_ = fd;
  }

  /// Pointer to the hosting actor system.
  multiplexer* mpx_;

  /// Callback for errors.
  std::function<void(const error&)> do_on_error_;

  /// Configures the maximum number of concurrent connections.
  size_t max_connections_ = defaults::net::max_connections.fallback;

  /// User-defined state for getting things up and running.
  std::variant<none_t, config, tcp_accept_socket> state_;

  /// Pointer to the (optional) SSL context.
  std::shared_ptr<ssl::context> ctx_;
};

} // namespace caf::net::lp
