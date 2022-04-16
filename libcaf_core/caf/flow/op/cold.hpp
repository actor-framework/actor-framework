// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/plain_ref_counted.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/op/base.hpp"
#include "caf/flow/subscription.hpp"

namespace caf::flow::op {

/// Convenience base type for *cold* observable types.
template <class T>
class cold : public detail::plain_ref_counted, public base<T> {
public:
  // -- member types -----------------------------------------------------------

  using output_type = T;

  // -- constructors, destructors, and assignment operators --------------------

  explicit cold(coordinator* ctx) : ctx_(ctx) {
    // nop
  }

  // -- implementation of disposable_impl --------------------------------------

  void ref_coordinated() const noexcept final {
    this->ref();
  }

  void deref_coordinated() const noexcept final {
    this->deref();
  }

  // -- implementation of observable_impl<T> -----------------------------------

  coordinator* ctx() const noexcept final {
    return ctx_;
  }

protected:
  // -- member variables -------------------------------------------------------

  coordinator* ctx_;
};

} // namespace caf::flow::op
