// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/disposable.hpp"
#include "caf/error.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/op/cold.hpp"

namespace caf::flow::op {

/// Emits an error to the subscriber immediately after subscribing.
template <class T>
class fail : public cold<T> {
public:
  // -- member types -----------------------------------------------------------

  using super = cold<T>;

  // -- constructors, destructors, and assignment operators --------------------

  fail(coordinator* parent, error err) : super(parent), err_(std::move(err)) {
    // nop
  }

  // -- implementation of observable_impl<T> -----------------------------------

  disposable subscribe(observer<T> out) override {
    return super::fail_subscription(out, err_);
  }

private:
  error err_;
};

} // namespace caf::flow::op
