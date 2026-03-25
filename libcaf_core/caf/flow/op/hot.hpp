// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/atomic_ref_count.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/op/base.hpp"
#include "caf/flow/subscription.hpp"

namespace caf::flow::op {

/// Convenience base type for *hot* observable types.
template <class T>
class hot : public base<T> {
public:
  // -- constructors, destructors, and assignment operators --------------------

  explicit hot(coordinator* parent) : parent_(parent) {
    // nop
  }

  // -- implementation of disposable_impl --------------------------------------

  void ref_coordinated() const noexcept override {
    ref_count_.inc();
  }

  void deref_coordinated() const noexcept override {
    ref_count_.dec(this);
  }

  // -- implementation of observable_impl<T> -----------------------------------

  coordinator* parent() const noexcept override {
    return parent_;
  }

protected:
  // -- member variables -------------------------------------------------------

  mutable detail::atomic_ref_count ref_count_;

  coordinator* parent_;
};

} // namespace caf::flow::op
