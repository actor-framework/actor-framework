// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/dsl/base.hpp"
#include "caf/net/dsl/client_config.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/ssl/tcp_acceptor.hpp"
#include "caf/net/tcp_accept_socket.hpp"

#include "caf/config_value.hpp"
#include "caf/make_counted.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace caf::net::dsl {

/// Base type for client factories for use with `has_connect`.
template <class Config, class Derived>
class client_factory_base {
public:
  using config_type = Config;

  using config_pointer = intrusive_ptr<config_type>;

  client_factory_base(const client_factory_base&) = default;

  client_factory_base& operator=(const client_factory_base&) = default;

  template <class T, class... Ts>
  explicit client_factory_base(dsl::client_config_tag<T> token, Ts&&... xs) {
    cfg_ = config_type::make(token, std::forward<Ts>(xs)...);
  }

  explicit client_factory_base(config_pointer cfg) : cfg_(std::move(cfg)) {
    // nop
  }

  /// Sets the callback for errors.
  template <class F>
  Derived& do_on_error(F callback) {
    static_assert(std::is_invocable_v<F, const error&>);
    cfg_->on_error = make_shared_type_erased_callback(std::move(callback));
    return dref();
  }

  /// Sets the retry delay for connection attempts.
  ///
  /// @param value The new retry delay.
  /// @returns a reference to this `client_factory`.
  Derived& retry_delay(timespan value) {
    if (auto* lazy = get_if<client_config::lazy>(&cfg_->data))
      lazy->retry_delay = value;
    return dref();
  }

  /// Sets the connection timeout for connection attempts.
  ///
  /// @param value The new connection timeout.
  /// @returns a reference to this `client_factory`.
  Derived& connection_timeout(timespan value) {
    if (auto* lazy = get_if<client_config::lazy>(&cfg_->data))
      lazy->connection_timeout = value;
    return dref();
  }

  /// Sets the maximum number of connection retry attempts.
  ///
  /// @param value The new maximum retry count.
  /// @returns a reference to this `client_factory`.
  Derived& max_retry_count(size_t value) {
    if (auto* lazy = get_if<client_config::lazy>(&cfg_->data))
      lazy->max_retry_count = value;
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
  auto with_ssl_connection(Fn&& fn) {
    return [this, fn = std::forward<Fn>(fn)](auto&& fd) mutable {
      using fd_t = decltype(fd);
      using res_t = decltype(fn(std::forward<fd_t>(fd)));
      auto* sub = cfg_->as_has_make_ctx();
      if (sub == nullptr) {
        auto err = error{sec::logic_error,
                         "required SSL but no context available"};
        return res_t{std::move(err)};
      }
      std::shared_ptr<ssl::context> ctx;
      if (auto& make_ctx = sub->make_ctx) {
        auto maybe_ctx = make_ctx();
        if (!maybe_ctx)
          return res_t{maybe_ctx.error()};
        ctx = std::move(*maybe_ctx);
      } else {
        auto maybe_ctx = ssl::context::make_client(ssl::tls::v1_2);
        if (!maybe_ctx)
          return res_t{maybe_ctx.error()};
        ctx = std::make_shared<ssl::context>(std::move(*maybe_ctx));
      }
      auto conn = ctx->new_connection(std::forward<fd_t>(fd));
      if (!conn)
        return res_t{conn.error()};
      return fn(std::move(*conn));
    };
  }

  template <class Fn>
  auto with_ssl_connection_or_socket(Fn&& fn) {
    return [this, fn = std::forward<Fn>(fn)](auto&& fd) mutable {
      using fd_t = decltype(fd);
      using res_t = decltype(fn(std::forward<fd_t>(fd)));
      if (auto* sub = cfg_->as_has_make_ctx(); sub && sub->make_ctx) {
        auto& make_ctx = sub->make_ctx;
        auto maybe_ctx = make_ctx();
        if (!maybe_ctx)
          return res_t{maybe_ctx.error()};
        auto& ctx = *maybe_ctx;
        auto conn = ctx->new_connection(std::forward<fd_t>(fd));
        if (!conn)
          return res_t{conn.error()};
        return fn(std::move(*conn));
      }
      return fn(std::forward<fd_t>(fd));
    };
  }

  template <class Fn>
  auto with_ssl_connection_or_socket_select(bool use_ssl, Fn&& fn) {
    return [this, use_ssl, fn = std::forward<Fn>(fn)](auto&& fd) mutable {
      using fd_t = decltype(fd);
      if (use_ssl) {
        auto g = with_ssl_connection(std::move(fn));
        return g(std::forward<fd_t>(fd));
      }
      return fn(std::forward<fd_t>(fd));
    };
  }

  config_pointer cfg_;
};

} // namespace caf::net::dsl
