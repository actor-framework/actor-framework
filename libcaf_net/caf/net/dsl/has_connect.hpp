// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/make_counted.hpp"
#include "caf/net/dsl/base.hpp"
#include "caf/net/dsl/client_config.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/tcp_stream_socket.hpp"

#include <cstdint>
#include <string>

namespace caf::net::dsl {

/// DSL entry point for creating a client.
template <class Base, class Subtype>
class has_connect : public Base {
public:
  using trait_type = typename Base::trait_type;

  /// Creates a `connect_factory` object for the given TCP `host` and `port`.
  ///
  /// @param host The hostname or IP address to connect to.
  /// @param port The port number to connect to.
  /// @returns a `connect_factory` object initialized with the given parameters.
  auto connect(std::string host, uint16_t port) {
    auto& dref = static_cast<Subtype&>(*this);
    auto cfg = make_lazy_config(std::move(host), port);
    return dref.lift(dref.with_context(std::move(cfg)));
  }

  /// Creates a `connect_factory` object for the given stream `fd`.
  ///
  /// @param fd The stream socket to use for the connection.
  /// @returns a `connect_factory` object that will use the given socket.
  auto connect(stream_socket fd) {
    auto& dref = static_cast<Subtype&>(*this);
    auto cfg = make_socket_config(fd);
    return dref.lift(dref.with_context(std::move(cfg)));
  }

  /// Creates a `connect_factory` object for the given SSL `connection`.
  ///
  /// @param conn The SSL connection to use.
  /// @returns a `connect_factory` object that will use the given connection.
  auto connect(ssl::connection conn) {
    auto& dref = static_cast<Subtype&>(*this);
    auto cfg = make_conn_config(std::move(conn));
    return dref.lift(std::move(cfg));
  }

protected:
  template <class... Ts>
  auto make_lazy_config(Ts&&... xs) {
    using impl_t = lazy_client_config<trait_type>;
    return make_counted<impl_t>(this->mpx(), this->trait(),
                                std::forward<Ts>(xs)...);
  }

  template <class... Ts>
  auto make_socket_config(Ts&&... xs) {
    using impl_t = socket_client_config<trait_type>;
    return make_counted<impl_t>(this->mpx(), this->trait(),
                                std::forward<Ts>(xs)...);
  }

  template <class... Ts>
  auto make_conn_config(Ts&&... xs) {
    using impl_t = conn_client_config<trait_type>;
    return make_counted<impl_t>(this->mpx(), this->trait(),
                                std::forward<Ts>(xs)...);
  }

  template <class... Ts>
  auto make_fail_config(Ts&&... xs) {
    using impl_t = fail_client_config<trait_type>;
    return make_counted<impl_t>(this->mpx(), this->trait(),
                                std::forward<Ts>(xs)...);
  }
};

} // namespace caf::net::dsl
