// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/make_counted.hpp"
#include "caf/net/dsl/has_trait.hpp"
#include "caf/net/dsl/server_config.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/ssl/acceptor.hpp"
#include "caf/net/tcp_accept_socket.hpp"

#include <cstdint>
#include <string>

namespace caf::net::dsl {

/// DSL entry point for creating a server.
template <class ServerFactory>
class has_accept : public has_trait<typename ServerFactory::trait_type> {
public:
  using trait_type = typename ServerFactory::trait_type;

  using super = has_trait<trait_type>;

  using super::super;

  /// Creates an `accept_factory` object for the given TCP `port` and
  /// `bind_address`.
  ///
  /// @param port Port number to bind to.
  /// @param bind_address IP address to bind to. Default is an empty string.
  /// @returns an `accept_factory` object initialized with the given parameters.
  ServerFactory accept(uint16_t port, std::string bind_address = "") {
    auto cfg = make_lazy_config(port, std::move(bind_address));
    return ServerFactory{std::move(cfg)};
  }

  /// Creates an `accept_factory` object for the given TCP `port` and
  /// `bind_address`.
  ///
  /// @param ctx The SSL context for encryption.
  /// @param port Port number to bind to.
  /// @param bind_address IP address to bind to. Default is an empty string.
  /// @returns an `accept_factory` object initialized with the given parameters.
  ServerFactory accept(ssl::context ctx, uint16_t port,
                       std::string bind_address = "") {
    auto cfg = make_lazy_config(port, std::move(bind_address));
    cfg->ctx = std::make_shared<ssl::context>(std::move(ctx));
    return ServerFactory{std::move(cfg)};
  }

  /// Creates an `accept_factory` object for the given accept socket.
  ///
  /// @param fd File descriptor for the accept socket.
  /// @returns an `accept_factory` object that will start a Prometheus server on
  ///          the given socket.
  ServerFactory accept(tcp_accept_socket fd) {
    auto cfg = make_socket_config(fd);
    return ServerFactory{std::move(cfg)};
  }

  /// Creates an `accept_factory` object for the given acceptor.
  ///
  /// @param ctx The SSL context for encryption.
  /// @param fd File descriptor for the accept socket.
  /// @returns an `accept_factory` object that will start a Prometheus server on
  ///          the given acceptor.
  ServerFactory accept(ssl::context ctx, tcp_accept_socket fd) {
    auto cfg = make_socket_config(fd);
    cfg->ctx = std::make_shared<ssl::context>(std::move(ctx));
    return ServerFactory{std::move(cfg)};
  }

  /// Creates an `accept_factory` object for the given acceptor.
  ///
  /// @param acc The SSL acceptor for incoming connections.
  /// @returns an `accept_factory` object that will start a Prometheus server on
  ///          the given acceptor.
  ServerFactory accept(ssl::acceptor acc) {
    return accept(std::move(acc.ctx()), acc.fd());
  }

private:
  template <class... Ts>
  server_config_ptr<trait_type> make_lazy_config(Ts&&... xs) {
    using impl_t = typename server_config<trait_type>::lazy;
    return make_counted<impl_t>(this->mpx(), this->trait(),
                                std::forward<Ts>(xs)...);
  }

  template <class... Ts>
  server_config_ptr<trait_type> make_socket_config(Ts&&... xs) {
    using impl_t = typename server_config<trait_type>::socket;
    return make_counted<impl_t>(this->mpx(), this->trait(),
                                std::forward<Ts>(xs)...);
  }
};

} // namespace caf::net::dsl
