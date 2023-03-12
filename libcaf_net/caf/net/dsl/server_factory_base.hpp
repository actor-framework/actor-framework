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

/// Base type for server factories for use with `can_accept`.
template <class Trait, class Derived>
class server_factory_base {
public:
  using trait_type = Trait;

  explicit server_factory_base(server_config_ptr<Trait> cfg)
    : cfg_(std::move(cfg)) {
    // nop
  }

  server_factory_base(const server_factory_base&) = default;

  server_factory_base& operator=(const server_factory_base&) = default;

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
  Derived& reuse_addr(bool value) {
    if (auto* cfg = get_if<lazy_server_config<Trait>>(cfg_.get()))
      cfg->reuse_addr = value;
    return dref();
  }

  server_config<Trait>& config() {
    return *cfg_;
  }

protected:
  Derived& dref() {
    return static_cast<Derived&>(*this);
  }

  server_config_ptr<Trait> cfg_;
};

} // namespace caf::net::dsl
