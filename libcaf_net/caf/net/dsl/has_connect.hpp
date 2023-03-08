// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/make_counted.hpp"
#include "caf/net/dsl/client_config.hpp"
#include "caf/net/dsl/has_trait.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/tcp_stream_socket.hpp"
#include "caf/uri.hpp"

#include <cstdint>
#include <string>

namespace caf::net::dsl {

/// DSL entry point for creating a server.
template <class ClientFactory>
class has_connect : public has_trait<typename ClientFactory::trait_type> {
public:
  using trait_type = typename ClientFactory::trait_type;

  using super = has_trait<trait_type>;

  using super::super;

  /// Creates a `connect_factory` object for the given TCP `host` and `port`.
  ///
  /// @param host The hostname or IP address to connect to.
  /// @param port The port number to connect to.
  /// @returns a `connect_factory` object initialized with the given parameters.
  ClientFactory connect(std::string host, uint16_t port) {
    auto cfg = make_lazy_config(std::move(host), port);
    return ClientFactory{std::move(cfg)};
  }

  /// Creates a `connect_factory` object for the given SSL `context`, TCP
  /// `host`, and `port`.
  ///
  /// @param ctx The SSL context for encryption.
  /// @param host The hostname or IP address to connect to.
  /// @param port The port number to connect to.
  /// @returns a `connect_factory` object initialized with the given parameters.
  ClientFactory connect(ssl::context ctx, std::string host, uint16_t port) {
    auto cfg = make_lazy_config(std::move(host), port);
    cfg->ctx = std::make_shared<ssl::context>(std::move(ctx));
    return ClientFactory{std::move(cfg)};
  }

  /// Creates a `connect_factory` object for the given TCP `endpoint`.
  ///
  /// @param endpoint The endpoint of the TCP server to connect to.
  /// @returns a `connect_factory` object initialized with the given parameters.
  ClientFactory connect(const uri& endpoint) {
    auto cfg = make_lazy_config(endpoint);
    return ClientFactory{std::move(cfg)};
  }

  /// Creates a `connect_factory` object for the given SSL `context` and TCP
  /// `endpoint`.
  ///
  /// @param ctx The SSL context for encryption.
  /// @param endpoint The endpoint of the TCP server to connect to.
  /// @returns a `connect_factory` object initialized with the given parameters.
  ClientFactory connect(ssl::context ctx, const uri& endpoint) {
    auto cfg = make_lazy_config(endpoint);
    cfg->ctx = std::make_shared<ssl::context>(std::move(ctx));
    return ClientFactory{std::move(cfg)};
  }

  /// Creates a `connect_factory` object for the given TCP `endpoint`.
  ///
  /// @param endpoint The endpoint of the TCP server to connect to.
  /// @returns a `connect_factory` object initialized with the given parameters.
  ClientFactory connect(expected<uri> endpoint) {
    if (endpoint)
      return connect(*endpoint);
    auto cfg = make_fail_config(endpoint.error());
    return ClientFactory{std::move(cfg)};
  }

  /// Creates a `connect_factory` object for the given SSL `context` and TCP
  /// `endpoint`.
  ///
  /// @param ctx The SSL context for encryption.
  /// @param endpoint The endpoint of the TCP server to connect to.
  /// @returns a `connect_factory` object initialized with the given parameters.
  ClientFactory connect(ssl::context ctx, expected<uri> endpoint) {
    if (endpoint)
      return connect(std::move(ctx), *endpoint);
    auto cfg = make_fail_config(endpoint.error());
    return ClientFactory{std::move(cfg)};
  }

  /// Creates a `connect_factory` object for the given stream `fd`.
  ///
  /// @param fd The stream socket to use for the connection.
  /// @returns a `connect_factory` object that will use the given socket.
  ClientFactory connect(stream_socket fd) {
    auto cfg = make_socket_config(fd);
    return ClientFactory{std::move(cfg)};
  }

  /// Creates a `connect_factory` object for the given SSL `connection`.
  ///
  /// @param conn The SSL connection to use.
  /// @returns a `connect_factory` object that will use the given connection.
  ClientFactory connect(ssl::connection conn) {
    auto cfg = make_conn_config(std::move(conn));
    return ClientFactory{std::move(cfg)};
  }

private:
  template <class... Ts>
  client_config_ptr<trait_type> make_lazy_config(Ts&&... xs) {
    using impl_t = lazy_client_config<trait_type>;
    return make_counted<impl_t>(this->mpx(), this->trait(),
                                std::forward<Ts>(xs)...);
  }

  template <class... Ts>
  client_config_ptr<trait_type> make_socket_config(Ts&&... xs) {
    using impl_t = socket_client_config<trait_type>;
    return make_counted<impl_t>(this->mpx(), this->trait(),
                                std::forward<Ts>(xs)...);
  }

  template <class... Ts>
  client_config_ptr<trait_type> make_conn_config(Ts&&... xs) {
    using impl_t = conn_client_config<trait_type>;
    return make_counted<impl_t>(this->mpx(), this->trait(),
                                std::forward<Ts>(xs)...);
  }

  template <class... Ts>
  client_config_ptr<trait_type> make_fail_config(Ts&&... xs) {
    using impl_t = fail_client_config<trait_type>;
    return make_counted<impl_t>(this->mpx(), this->trait(),
                                std::forward<Ts>(xs)...);
  }
};

} // namespace caf::net::dsl
