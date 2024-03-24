// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/dsl/base.hpp"
#include "caf/net/dsl/generic_config.hpp"
#include "caf/net/dsl/has_accept.hpp"
#include "caf/net/dsl/has_connect.hpp"
#include "caf/net/dsl/has_context.hpp"
#include "caf/net/dsl/has_lazy_context.hpp"
#include "caf/net/dsl/has_uri_connect.hpp"
#include "caf/net/http/client_factory.hpp"
#include "caf/net/http/config.hpp"
#include "caf/net/http/request.hpp"
#include "caf/net/http/router.hpp"
#include "caf/net/http/server_factory.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/ssl/context.hpp"
#include "caf/net/tcp_accept_socket.hpp"

#include "caf/extend.hpp"
#include "caf/fwd.hpp"

#include <cstdint>

namespace caf::net::http {

/// Entry point for the `with(...)` DSL.
class with_t : public extend<dsl::base, with_t>:: //
               with<dsl::has_accept, dsl::has_lazy_context, dsl::has_context,
                    dsl::has_uri_connect> {
public:
  using config_type = base_config;

  template <class... Ts>
  explicit with_t(multiplexer* mpx) : config_(make_counted<base_config>(mpx)) {
    // nop
  }

  with_t(with_t&&) noexcept = default;

  with_t(const with_t&) noexcept = default;

  with_t& operator=(with_t&&) noexcept = default;

  with_t& operator=(const with_t&) noexcept = default;

  /// @private
  base_config& config() {
    return *config_;
  }

  /// @private
  template <class T, class... Ts>
  auto make(dsl::server_config_tag<T> token, Ts&&... xs) {
    return server_factory{token, *config_, std::forward<Ts>(xs)...};
  }

  /// @private
  template <class T, class... Ts>
  auto make(dsl::client_config_tag<T> token, Ts&&... xs) {
    return client_factory{token, *config_, std::forward<Ts>(xs)...};
  }

private:
  intrusive_ptr<base_config> config_;
};

inline with_t with(actor_system& sys) {
  return with_t{multiplexer::from(sys)};
}

inline with_t with(multiplexer* mpx) {
  return with_t{mpx};
}

} // namespace caf::net::http
