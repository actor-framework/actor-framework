// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

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
    return base_config().with_ssl_acceptor_or_socket(std::forward<Fn>(fn));
  }

  virtual server_config_value& base_config() = 0;
};

} // namespace caf::net::dsl
