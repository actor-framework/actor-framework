// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/fwd.hpp"
#include "caf/net/dsl/has_accept.hpp"
#include "caf/net/dsl/has_connect.hpp"
#include "caf/net/lp/client_factory.hpp"
#include "caf/net/lp/server_factory.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/ssl/acceptor.hpp"
#include "caf/net/ssl/context.hpp"
#include "caf/net/tcp_accept_socket.hpp"

#include <cstdint>

namespace caf::net::lp {

/// Entry point for the `with(...)` DSL.
template <class Trait>
class with_t : public dsl::has_accept<server_factory<Trait>>,
               public dsl::has_connect<client_factory<Trait>> {
public:
  template <class... Ts>
  explicit with_t(multiplexer* mpx, Ts&&... xs)
    : mpx_(mpx), trait_(std::forward<Ts>(xs)...) {
    // nop
  }

  with_t(const with_t&) noexcept = default;

  with_t& operator=(const with_t&) noexcept = default;

  multiplexer* mpx() const noexcept override {
    return mpx_;
  }

  const Trait& trait() const noexcept override {
    return trait_;
  }

private:
  /// Pointer to multiplexer that runs the protocol stack.
  multiplexer* mpx_;

  /// User-defined trait for configuring serialization.
  Trait trait_;
};

template <class Trait = binary::default_trait>
with_t<Trait> with(actor_system& sys) {
  return with_t<Trait>{multiplexer::from(sys)};
}

template <class Trait = binary::default_trait>
with_t<Trait> with(multiplexer* mpx) {
  return with_t<Trait>{mpx};
}

} // namespace caf::net::lp
