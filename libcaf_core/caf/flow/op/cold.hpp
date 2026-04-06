// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/atomic_ref_count.hpp"
#include "caf/flow/op/base.hpp"

namespace caf::flow::op {

/// Convenience base type for *cold* observable types.
template <class T>
class cold : public base<T> {
public:
  // -- constructors, destructors, and assignment operators --------------------

  explicit cold(coordinator* parent) : parent_(parent) {
    // nop
  }

  // -- implementation of observable_impl<T> -----------------------------------

  coordinator* parent() const noexcept override {
    return parent_;
  }

protected:
  // -- member variables -------------------------------------------------------
  coordinator* parent_;
};

} // namespace caf::flow::op
