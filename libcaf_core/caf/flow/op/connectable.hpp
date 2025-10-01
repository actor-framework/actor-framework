// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/flow/op/base.hpp"
#include "caf/fwd.hpp"

namespace caf::flow::op {

/// Abstract base type for all flow operators that implement the *connectable*
/// concept. A connectable observable is an observable that does not emit items
/// until connected to a source observable.
template <class T>
class connectable : public base<T> {
public:
  /// Connects to the source @ref observable, thus starting to emit items.
  virtual disposable connect() = 0;

  /// Checks whether this operator is connected to its source observable.
  virtual bool connected() const noexcept = 0;
};

} // namespace caf::flow::op
