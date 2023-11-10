// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/dsl/base.hpp"
#include "caf/net/dsl/server_config.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/ssl/tcp_acceptor.hpp"
#include "caf/net/tcp_accept_socket.hpp"

#include "caf/make_counted.hpp"

#include <cstdint>
#include <string>

namespace caf::net::dsl {

/// DSL entry point for creating a server.
template <class Base, class Subtype>
class has_accept : public Base {
public:
  /// Creates an `accept_factory` object for the given TCP `port` and
  /// `bind_address`.
  ///
  /// @param port Port number to bind to.
  /// @param bind_address IP address to bind to. Default is an empty string.
  /// @returns an `accept_factory` object initialized with the given parameters.
  auto accept(uint16_t port, std::string bind_address = "") {
    auto& dref = static_cast<Subtype&>(*this);
    return dref.make(server_config::lazy_v, port, std::move(bind_address));
  }

  /// Creates an `accept_factory` object for the given accept socket.
  ///
  /// @param fd File descriptor for the accept socket.
  /// @returns an `accept_factory` object that will start a server on `fd`.
  auto accept(tcp_accept_socket fd) {
    auto& dref = static_cast<Subtype&>(*this);
    return dref.make(server_config::socket_v, fd);
  }

  /// Creates an `accept_factory` object for the given acceptor.
  ///
  /// @param acc The SSL acceptor for incoming TCP connections.
  /// @returns an `accept_factory` object that will start a server on `acc`.
  auto accept(ssl::tcp_acceptor acc) {
    auto& dref = static_cast<Subtype&>(*this);
    auto& cfg = dref.config();
    auto ptr = cfg.as_has_make_ctx();
    // The SSL acceptor has its own context, we cannot have two.
    if (!ptr) {
      return dref.make(server_config::fail_v, cfg, cfg.cannot_add_ctx());
    } else if (ptr->ctx) {
      auto err = make_error(sec::logic_error,
                            "passed an ssl::tcp_acceptor to a factory "
                            "with a valid SSL context");
      return dref.make(server_config::fail_v, std::move(err));
    } else {
      ptr->ctx = std::make_shared<ssl::context>(std::move(acc.ctx()));
      return accept(acc.fd());
    }
  }
};

} // namespace caf::net::dsl
