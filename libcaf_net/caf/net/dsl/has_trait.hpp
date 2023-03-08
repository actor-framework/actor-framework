// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/fwd.hpp"

namespace caf::net::dsl {

/// Base type for DSL classes.
template <class Trait>
class has_trait {
public:
  virtual ~has_trait() {
    // nop
  }

  /// @returns the pointer to the @ref multiplexer.
  virtual multiplexer* mpx() const noexcept = 0;

  /// @returns the trait object.
  virtual const Trait& trait() const noexcept = 0;
};

} // namespace caf::net::dsl
