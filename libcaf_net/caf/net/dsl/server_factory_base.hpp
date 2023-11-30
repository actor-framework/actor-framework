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
template <class Config, class Derived>
class server_factory_base {
public:
  using config_type = Config;

  using config_pointer = intrusive_ptr<config_type>;

  server_factory_base(server_factory_base&&) = default;

  server_factory_base(const server_factory_base&) = default;

  server_factory_base& operator=(server_factory_base&&) = default;

  server_factory_base& operator=(const server_factory_base&) = default;

  explicit server_factory_base(config_pointer cfg) : cfg_(std::move(cfg)) {
    // nop
  }

  template <class T, class... Ts>
  explicit server_factory_base(dsl::server_config_tag<T> token, Ts&&... xs) {
    cfg_ = config_type::make(token, std::forward<Ts>(xs)...);
  }

  /// Sets the callback for errors.
  template <class F>
  Derived& do_on_error(F callback) {
    static_assert(std::is_invocable_v<F, const error&>);
    cfg_->on_error = make_shared_type_erased_callback(std::move(callback));
    return dref();
  }

  /// Configures how many concurrent connections the server accepts.
  Derived& max_connections(size_t value) {
    cfg_->max_connections = value;
    return dref();
  }

  /// Configures whether the server creates its socket with `SO_REUSEADDR`.
  Derived& reuse_address(bool value) {
    if (auto* lazy = get_if<server_config::lazy>(&cfg_->data))
      lazy->reuse_addr = value;
    return dref();
  }

  config_type& config() {
    return *cfg_;
  }

protected:
  Derived& dref() {
    return static_cast<Derived&>(*this);
  }

  template <class Fn>
  auto with_ssl_acceptor_or_socket(Fn&& fn) {
    return [this, fn = std::forward<Fn>(fn)](auto&& fd) mutable {
      using fd_t = decltype(fd);
      using res_t = decltype(fn(std::forward<fd_t>(fd)));
      if (auto* sub = cfg_->as_has_make_ctx(); sub && sub->make_ctx_valid()) {
        auto& make_ctx = sub->make_ctx();
        auto ctx = make_ctx();
        if (!ctx)
          return res_t{ctx.error()};
        auto acc = ssl::tcp_acceptor{std::forward<fd_t>(fd), std::move(*ctx)};
        return fn(std::move(acc));
      }
      return fn(std::forward<fd_t>(fd));
    };
  }

  config_pointer cfg_;
};

} // namespace caf::net::dsl
