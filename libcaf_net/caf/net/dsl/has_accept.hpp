// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/make_counted.hpp"
#include "caf/net/dsl/base.hpp"
#include "caf/net/dsl/server_config.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/ssl/acceptor.hpp"
#include "caf/net/tcp_accept_socket.hpp"

#include <cstdint>
#include <string>

namespace caf::net::dsl {

/// DSL entry point for creating a server.
template <class Base, class Subtype>
class has_accept : public Base {
public:
  using trait_type = typename Base::trait_type;

  /// Creates an `accept_factory` object for the given TCP `port` and
  /// `bind_address`.
  ///
  /// @param port Port number to bind to.
  /// @param bind_address IP address to bind to. Default is an empty string.
  /// @returns an `accept_factory` object initialized with the given parameters.
  auto accept(uint16_t port, std::string bind_address = "") {
    auto& dref = static_cast<Subtype&>(*this);
    auto cfg = make_lazy_config(port, std::move(bind_address));
    return dref.lift(dref.with_context(std::move(cfg)));
  }

  /// Creates an `accept_factory` object for the given accept socket.
  ///
  /// @param fd File descriptor for the accept socket.
  /// @returns an `accept_factory` object that will start a server on `fd`.
  auto accept(tcp_accept_socket fd) {
    auto& dref = static_cast<Subtype&>(*this);
    return dref.lift(dref.with_context(make_socket_config(fd)));
  }

  /// Creates an `accept_factory` object for the given acceptor.
  ///
  /// @param acc The SSL acceptor for incoming connections.
  /// @returns an `accept_factory` object that will start a server on `acc`.
  auto accept(ssl::acceptor acc) {
    auto& dref = static_cast<Subtype&>(*this);
    // The SSL acceptor has its own context, we cannot have two.
    auto& ctx = dref().context();
    if (ctx.has_value()) {
      auto err = make_error(
        sec::logic_error,
        "passed an ssl::acceptor to a factory with a valid SSL context");
      return dref.lift(make_fail_config(std::move(err)));
    }
    // Forward an already existing error.
    if (ctx.error()) {
      return dref.lift(make_fail_config(std::move(ctx.error())));
    }
    // Default-constructed error means: "no SSL". Use he one from the acceptor.
    ctx = std::move(acc.ctx());
    return accept(acc.fd());
  }

protected:
  using config_base_type = dsl::config_with_trait<trait_type>;

  template <class... Ts>
  auto make_lazy_config(Ts&&... xs) {
    using impl_t = lazy_server_config<config_base_type>;
    return make_counted<impl_t>(std::forward<Ts>(xs)..., this->mpx(),
                                this->trait());
  }

  template <class... Ts>
  auto make_socket_config(Ts&&... xs) {
    using impl_t = socket_server_config<config_base_type>;
    return make_counted<impl_t>(std::forward<Ts>(xs)..., this->mpx(),
                                this->trait());
  }

  template <class... Ts>
  auto make_fail_config(Ts&&... xs) {
    using impl_t = fail_server_config<config_base_type>;
    return make_counted<impl_t>(std::forward<Ts>(xs)..., this->mpx(),
                                this->trait());
  }
};

} // namespace caf::net::dsl
