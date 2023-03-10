// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/make_counted.hpp"
#include "caf/net/dsl/base.hpp"
#include "caf/net/dsl/client_config.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/ssl/acceptor.hpp"
#include "caf/net/tcp_accept_socket.hpp"

#include <cstdint>
#include <string>

namespace caf::net::dsl {

/// Base type for client factories for use with `can_connect`.
template <class Trait, class Derived>
class client_factory_base {
public:
  using trait_type = Trait;

  explicit client_factory_base(client_config_ptr<Trait> cfg)
    : cfg_(std::move(cfg)) {
    // nop
  }

  client_factory_base(const client_factory_base&) = default;

  client_factory_base& operator=(const client_factory_base&) = default;

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
    if (auto* cfg = get_if<lazy_client_config<Trait>>(cfg_.get()))
      cfg->retry_delay = value;
    return dref();
  }

  /// Sets the connection timeout for connection attempts.
  ///
  /// @param value The new connection timeout.
  /// @returns a reference to this `client_factory`.
  Derived& connection_timeout(timespan value) {
    if (auto* cfg = get_if<lazy_client_config<Trait>>(cfg_.get()))
      cfg->connection_timeout = value;
    return dref();
  }

  /// Sets the maximum number of connection retry attempts.
  ///
  /// @param value The new maximum retry count.
  /// @returns a reference to this `client_factory`.
  Derived& max_retry_count(size_t value) {
    if (auto* cfg = get_if<lazy_client_config<Trait>>(cfg_.get()))
      cfg->max_retry_count = value;
    return dref();
  }

  client_config<Trait>& config() {
    return *cfg_;
  }

private:
  Derived& dref() {
    return static_cast<Derived&>(*this);
  }

  client_config_ptr<Trait> cfg_;
};

} // namespace caf::net::dsl
