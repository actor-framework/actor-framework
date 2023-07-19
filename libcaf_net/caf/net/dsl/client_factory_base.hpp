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
#include <string>

namespace caf::net::dsl {

/// Base type for client factories for use with `can_connect`.
template <class Config, class Derived>
class client_factory_base {
public:
  using config_type = Config;

  using trait_type = typename config_type::trait_type;

  using config_pointer = intrusive_ptr<config_type>;

  client_factory_base(const client_factory_base&) = default;

  client_factory_base& operator=(const client_factory_base&) = default;

  template <class T, class... Ts>
  explicit client_factory_base(dsl::client_config_tag<T> token, Ts&&... xs) {
    cfg_ = config_type::make(token, std::forward<Ts>(xs)...);
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

private:
  Derived& dref() {
    return static_cast<Derived&>(*this);
  }

  config_pointer cfg_;
};

} // namespace caf::net::dsl
