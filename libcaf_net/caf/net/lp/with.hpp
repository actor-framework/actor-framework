// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/fwd.hpp"
#include "caf/net/lp/accept_factory.hpp"
#include "caf/net/lp/connect_factory.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/ssl/acceptor.hpp"
#include "caf/net/ssl/context.hpp"
#include "caf/net/tcp_accept_socket.hpp"

#include <cstdint>

namespace caf::net::lp {

/// Entry point for the `with(...)` DSL.
template <class Trait>
class with_t {
public:
  explicit with_t(multiplexer* mpx) : mpx_(mpx) {
    // nop
  }

  with_t(const with_t&) noexcept = default;

  with_t& operator=(const with_t&) noexcept = default;

  /// Creates an `accept_factory` object for the given TCP `port` and
  /// `bind_address`.
  ///
  /// @param port Port number to bind to.
  /// @param bind_address IP address to bind to. Default is an empty string.
  /// @param reuse_addr Whether or not to set `SO_REUSEADDR`.
  /// @returns an `accept_factory` object initialized with the given parameters.
  accept_factory<Trait> accept(uint16_t port, std::string bind_address = "",
                               bool reuse_addr = true) {
    accept_factory<Trait> factory{mpx_};
    factory.init(port, std::move(bind_address), std::move(reuse_addr));
    return factory;
  }

  /// Creates an `accept_factory` object for the given accept socket.
  ///
  /// @param fd File descriptor for the accept socket.
  /// @returns an `accept_factory` object that will start a Prometheus server on
  ///          the given socket.
  accept_factory<Trait> accept(tcp_accept_socket fd) {
    accept_factory<Trait> factory{mpx_};
    factory.init(fd);
    return factory;
  }

  /// Creates an `accept_factory` object for the given acceptor.
  ///
  /// @param acc The SSL acceptor for incoming connections.
  /// @returns an `accept_factory` object that will start a Prometheus server on
  ///          the given acceptor.
  accept_factory<Trait> accept(ssl::acceptor acc) {
    accept_factory<Trait> factory{mpx_};
    factory.set_ssl(std::move(std::move(acc.ctx())));
    factory.init(acc.fd());
    return factory;
  }

  /// Creates an `accept_factory` object for the given TCP `port` and
  /// `bind_address`.
  ///
  /// @param ctx The SSL context for encryption.
  /// @param port Port number to bind to.
  /// @param bind_address IP address to bind to. Default is an empty string.
  /// @param reuse_addr Whether or not to set `SO_REUSEADDR`.
  /// @returns an `accept_factory` object initialized with the given parameters.
  accept_factory<Trait> accept(ssl::context ctx, uint16_t port,
                               std::string bind_address = "",
                               bool reuse_addr = true) {
    accept_factory<Trait> factory{mpx_};
    factory.set_ssl(std::move(std::move(ctx)));
    factory.init(port, std::move(bind_address), std::move(reuse_addr));
    return factory;
  }

  /// Creates a `connect_factory` object for the given TCP `host` and `port`.
  ///
  /// @param host The hostname or IP address to connect to.
  /// @param port The port number to connect to.
  /// @returns a `connect_factory` object initialized with the given parameters.
  connect_factory<Trait> connect(std::string host, uint16_t port) {
    connect_factory<Trait> factory{mpx_};
    factory.init(std::move(host), port);
    return factory;
  }

  /// Creates a `connect_factory` object for the given SSL `context`, TCP
  /// `host`, and `port`.
  ///
  /// @param ctx The SSL context for encryption.
  /// @param host The hostname or IP address to connect to.
  /// @param port The port number to connect to.
  /// @returns a `connect_factory` object initialized with the given parameters.
  connect_factory<Trait> connect(ssl::context ctx, std::string host,
                                 uint16_t port) {
    connect_factory<Trait> factory{mpx_};
    factory.set_ssl(std::move(ctx));
    factory.init(std::move(host), port);
    return factory;
  }

  /// Creates a `connect_factory` object for the given TCP `endpoint`.
  ///
  /// @param endpoint The endpoint of the TCP server to connect to.
  /// @returns a `connect_factory` object initialized with the given parameters.
  connect_factory<Trait> connect(const uri& endpoint) {
    return connect_impl(nullptr, endpoint);
  }

  /// Creates a `connect_factory` object for the given SSL `context` and TCP
  /// `endpoint`.
  ///
  /// @param ctx The SSL context for encryption.
  /// @param endpoint The endpoint of the TCP server to connect to.
  /// @returns a `connect_factory` object initialized with the given parameters.
  connect_factory<Trait> connect(ssl::context ctx, const uri& endpoint) {
    return connect_impl(&ctx, endpoint);
  }

  /// Creates a `connect_factory` object for the given TCP `endpoint`.
  ///
  /// @param endpoint The endpoint of the TCP server to connect to.
  /// @returns a `connect_factory` object initialized with the given parameters.
  connect_factory<Trait> connect(expected<uri> endpoint) {
    if (endpoint)
      return connect_impl(nullptr, std::move(*endpoint));
    return connect_factory<Trait>{std::move(endpoint.error())};
  }

  /// Creates a `connect_factory` object for the given SSL `context` and TCP
  /// `endpoint`.
  ///
  /// @param ctx The SSL context for encryption.
  /// @param endpoint The endpoint of the TCP server to connect to.
  /// @returns a `connect_factory` object initialized with the given parameters.
  connect_factory<Trait> connect(ssl::context ctx, expected<uri> endpoint) {
    if (endpoint)
      return connect_impl(&ctx, std::move(*endpoint));
    return connect_factory<Trait>{std::move(endpoint.error())};
  }

  /// Creates a `connect_factory` object for the given stream `fd`.
  ///
  /// @param fd The stream socket to use for the connection.
  /// @returns a `connect_factory` object that will use the given socket.
  connect_factory<Trait> connect(stream_socket fd) {
    connect_factory<Trait> factory{mpx_};
    factory.init(fd);
    return factory;
  }

  /// Creates a `connect_factory` object for the given SSL `connection`.
  ///
  /// @param conn The SSL connection to use.
  /// @returns a `connect_factory` object that will use the given connection.
  connect_factory<Trait> connect(ssl::connection conn) {
    connect_factory<Trait> factory{mpx_};
    factory.init(std::move(conn));
    return factory;
  }

private:
  connect_factory<Trait> connect_impl(ssl::context* ctx, const uri& endpoint) {
    if (endpoint.scheme() != "tcp" || endpoint.authority().empty()) {
      auto err = make_error(sec::invalid_argument,
                            "lp::connect expects tcp://<host>:<port> URIs");
      return connect_factory<Trait>{mpx_, std::move(err)};
    }
    if (endpoint.authority().port == 0) {
      auto err = make_error(sec::invalid_argument,
                            "lp::connect expects URIs with a non-zero port");
      return connect_factory<Trait>{mpx_, std::move(err)};
    }
    connect_factory<Trait> factory{mpx_};
    if (ctx != nullptr) {
      factory.set_ssl(std::move(*ctx));
    }
    factory.init(endpoint.authority().host_str(), endpoint.authority().port);
    return factory;
  }

  /// Pointer to multiplexer that runs the protocol stack.
  multiplexer* mpx_;
};

template <class Trait = binary::default_trait>
with_t<Trait> with(actor_system& sys) {
  return with_t<Trait>{multiplexer::from(sys)};
}

template <class Trait = binary::default_trait>
with_t<Trait> with(multiplexer* mpx) {
  return with_t<Trait>{mpx};
}

} // namespace caf::net::lp
