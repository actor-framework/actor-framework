// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/fwd.hpp"
#include "caf/net/dsl/base.hpp"
#include "caf/net/dsl/generic_config.hpp"
#include "caf/net/dsl/has_accept.hpp"
#include "caf/net/dsl/has_connect.hpp"
#include "caf/net/dsl/has_context.hpp"
#include "caf/net/lp/client_factory.hpp"
#include "caf/net/lp/server_factory.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/ssl/context.hpp"
#include "caf/net/tcp_accept_socket.hpp"

#include <cstdint>

namespace caf::net::lp {

/// Entry point for the `with(...)` DSL.
template <class Trait>
class with_t : public extend<dsl::base, with_t<Trait>>::template //
               with<dsl::has_accept, dsl::has_connect, dsl::has_context> {
public:
  using config_type = dsl::generic_config_value<dsl::config_with_trait<Trait>>;

  template <class... Ts>
  explicit with_t(multiplexer* mpx) : config_(config_type::make(mpx)) {
    // nop
  }

  with_t(with_t&&) noexcept = default;

  with_t(const with_t&) noexcept = default;

  with_t& operator=(with_t&&) noexcept = default;

  with_t& operator=(const with_t&) noexcept = default;

  /// @private
  config_type& config() {
    return *config_;
  }

  /// @private
  template <class T, class... Ts>
  auto make(dsl::server_config_tag<T> token, Ts&&... xs) {
    return server_factory<Trait>{token, *config_, std::forward<Ts>(xs)...};
  }

  /// @private
  template <class T, class... Ts>
  auto make(dsl::client_config_tag<T> token, Ts&&... xs) {
    return client_factory<Trait>{token, *config_, std::forward<Ts>(xs)...};
  }

private:
  intrusive_ptr<config_type> config_;
};

template <class Trait = lp::default_trait>
with_t<Trait> with(actor_system& sys) {
  return with_t<Trait>{multiplexer::from(sys)};
}

template <class Trait = lp::default_trait>
with_t<Trait> with(multiplexer* mpx) {
  return with_t<Trait>{mpx};
}

} // namespace caf::net::lp
