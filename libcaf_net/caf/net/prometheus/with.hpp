// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/fwd.hpp"
#include "caf/net/prometheus/accept_factory.hpp"
#include "caf/net/ssl/acceptor.hpp"
#include "caf/net/ssl/context.hpp"
#include "caf/net/tcp_accept_socket.hpp"

#include <cstdint>

namespace caf::net::prometheus {

/// Entry point for the `with(...).accept(...).start()` DSL.
class with_t {
public:
  explicit with_t(actor_system* sys) : sys_(sys) {
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
  accept_factory accept(uint16_t port, std::string bind_address = "",
                        bool reuse_addr = true) {
    accept_factory factory{sys_};
    factory.init(port, std::move(bind_address), std::move(reuse_addr));
    return factory;
  }

  /// Creates an `accept_factory` object for the given accept socket.
  ///
  /// @param fd File descriptor for the accept socket.
  /// @returns an `accept_factory` object that will start a Prometheus server on
  ///          the given socket.
  accept_factory accept(tcp_accept_socket fd) {
    accept_factory factory{sys_};
    factory.init(fd);
    return factory;
  }

  /// Creates an `accept_factory` object for the given acceptor.
  ///
  /// @param acc The SSL acceptor for incoming connections.
  /// @returns an `accept_factory` object that will start a Prometheus server on
  ///          the given acceptor.
  accept_factory accept(ssl::acceptor acc) {
    accept_factory factory{sys_};
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
  accept_factory accept(ssl::context ctx, uint16_t port,
                        std::string bind_address = "", bool reuse_addr = true) {
    accept_factory factory{sys_};
    factory.set_ssl(std::move(std::move(ctx)));
    factory.init(port, std::move(bind_address), std::move(reuse_addr));
    return factory;
  }

private:
  /// Pointer to context.
  actor_system* sys_;
};

inline with_t with(actor_system& sys) {
  return with_t{&sys};
}

} // namespace caf::net::prometheus
