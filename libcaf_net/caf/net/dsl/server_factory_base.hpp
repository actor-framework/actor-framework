// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/dsl/base.hpp"
#include "caf/net/dsl/server_config.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/tcp_accept_socket.hpp"

#include "caf/config_value.hpp"
#include "caf/make_counted.hpp"

#include <cstdint>
#include <string>

namespace caf::net::dsl {

/// Base type for server factories for use with `can_accept`.
template <class Derived>
class server_factory_base {
public:
  virtual ~server_factory_base() {
    // nop
  }

  /// Sets the callback for errors.
  template <class F>
  Derived&& do_on_error(F callback) && {
    static_assert(std::is_invocable_v<F, const error&>);
    base_config().on_error
      = make_shared_type_erased_callback(std::move(callback));
    return dref();
  }

  /// Configures how many concurrent connections the server accepts.
  Derived&& max_connections(size_t value) && {
    base_config().max_connections = value;
    return dref();
  }

  /// Configures whether the server creates its socket with `SO_REUSEADDR`.
  Derived&& reuse_address(bool value) && {
    if (auto* lazy = get_if<server_config::lazy>(&base_config().data))
      lazy->reuse_addr = value;
    return dref();
  }

protected:
  Derived&& dref() {
    return std::move(static_cast<Derived&>(*this));
  }

  template <class Fn>
  auto with_ssl_acceptor_or_socket(Fn&& fn) {
    return [this, fn = std::forward<Fn>(fn)](auto&& fd) mutable {
      using fd_t = decltype(fd);
      using res_t = decltype(fn(std::forward<fd_t>(fd)));
      if (auto* sub = base_config().as_has_make_ctx(); sub && sub->make_ctx) {
        auto maybe_ctx = sub->make_ctx();
        if (!maybe_ctx)
          return res_t{maybe_ctx.error()};
        auto& ctx = *maybe_ctx;
        auto acc = ssl::tcp_acceptor{std::forward<fd_t>(fd), std::move(*ctx)};
        return fn(std::move(acc));
      }
      return fn(std::forward<fd_t>(fd));
    };
  }

  virtual server_config_value& base_config() = 0;
};

} // namespace caf::net::dsl
