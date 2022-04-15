// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/ref_counted_base.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/subscription.hpp"

namespace caf::flow::op {

/// Abstract base type for all flow operators.
template <class T>
class base : public coordinated {
public:
  /// The type of observed values.
  using output_type = T;

  /// Returns the @ref coordinator that executes this flow operator.
  virtual coordinator* ctx() const noexcept = 0;

  /// Subscribes a new observer to the operator.
  virtual disposable subscribe(observer<T> what) = 0;
};

} // namespace caf::flow::op
