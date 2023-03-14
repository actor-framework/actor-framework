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
  /// @param host The hostname or IP address to connect to.
  /// @param port The port number to connect to.
  /// @returns a `connect_factory` object initialized with the given parameters.
  auto connect(std::string host, uint16_t port) {
    auto& dref = static_cast<Subtype&>(*this);
    return dref.make(client_config_lazy_v, std::move(host), port, this->mpx(),
                     this->trait());
  }

  /// Creates a `connect_factory` object for the given stream `fd`.
  /// @param fd The stream socket to use for the connection.
  /// @returns a `connect_factory` object that will use the given socket.
  auto connect(stream_socket fd) {
    auto& dref = static_cast<Subtype&>(*this);
    return dref.make(client_config_socket_v, fd, this->mpx(), this->trait());
  }

  /// Creates a `connect_factory` object for the given SSL `connection`.
  /// @param conn The SSL connection to use.
  /// @returns a `connect_factory` object that will use the given connection.
  auto connect(ssl::connection conn) {
    auto& dref = static_cast<Subtype&>(*this);
    return dref.make(client_config_conn_v, std::move(conn), this->mpx(),
                     this->trait());
  }
};

} // namespace caf::net::dsl
