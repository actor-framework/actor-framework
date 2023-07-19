// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/dsl/generic_config.hpp"
#include "caf/net/dsl/has_accept.hpp"
#include "caf/net/dsl/has_context.hpp"
#include "caf/net/dsl/has_uri_connect.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/ssl/context.hpp"
#include "caf/net/tcp_accept_socket.hpp"
#include "caf/net/web_socket/client_factory.hpp"
#include "caf/net/web_socket/config.hpp"
#include "caf/net/web_socket/default_trait.hpp"
#include "caf/net/web_socket/has_on_request.hpp"

#include "caf/fwd.hpp"

#include <cstdint>

namespace caf::net::web_socket {

/// Entry point for the `with(...)` DSL.
template <class Trait>
class with_t : public extend<dsl::base, with_t<Trait>>::template //
               with<dsl::has_accept, dsl::has_uri_connect, dsl::has_context> {
public:
  using config_type = base_config<Trait>;

  template <class... Ts>
  explicit with_t(multiplexer* mpx) : config_(make_counted<config_type>(mpx)) {
    // nop
  }

  with_t(const with_t&) noexcept = default;

  with_t& operator=(const with_t&) noexcept = default;

  /// @private
  config_type& config() {
    return *config_;
  }

  /// @private
  template <class T, class... Ts>
  auto make(dsl::server_config_tag<T> token, Ts&&... xs) {
    return has_on_request<Trait>{token, *config_, std::forward<Ts>(xs)...};
  }

  /// @private
  template <class T, class... Ts>
  auto make(dsl::client_config_tag<T> token, Ts&&... xs) {
    return client_factory<Trait>{token, *config_, std::forward<Ts>(xs)...};
  }

private:
  intrusive_ptr<config_type> config_;
};

template <class Trait = default_trait>
with_t<Trait> with(actor_system& sys) {
  return with_t<Trait>{multiplexer::from(sys)};
}

template <class Trait = default_trait>
with_t<Trait> with(multiplexer* mpx) {
  return with_t<Trait>{mpx};
}

} // namespace caf::net::web_socket
