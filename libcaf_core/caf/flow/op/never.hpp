// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/flow/observer.hpp"
#include "caf/flow/op/cold.hpp"
#include "caf/flow/subscription.hpp"

#include <utility>

namespace caf::flow::op {

template <class T>
class never_sub : public subscription::impl_base {
public:
  // -- constructors, destructors, and assignment operators --------------------

  never_sub(coordinator* ctx, observer<T> out)
    : ctx_(ctx), out_(std::move(out)) {
    // nop
  }

  // -- implementation of subscription -----------------------------------------

  bool disposed() const noexcept override {
    return !out_;
  }

  void dispose() override {
    if (out_)
      ctx_->delay_fn([out = std::move(out_)]() mutable { out.on_complete(); });
  }

  void request(size_t) override {
    // nop
  }

private:
  /// Stores the context (coordinator) that runs this flow.
  coordinator* ctx_;

  /// Stores a handle to the subscribed observer.
  observer<T> out_;
};

/// An observable that never calls any callbacks on its subscribers.
template <class T>
class never : public cold<T> {
public:
  // -- member types -----------------------------------------------------------

  using super = cold<T>;

  using output_type = T;

  // -- constructors, destructors, and assignment operators --------------------

  explicit never(coordinator* ctx) : super(ctx) {
    // nop
  }

  // -- implementation of observable_impl<T> -----------------------------------

  disposable subscribe(observer<output_type> out) override {
    auto ptr = make_counted<never_sub<T>>(super::ctx_, out);
    out.ptr()->on_subscribe(subscription{ptr});
    return ptr->as_disposable();
  }
};

} // namespace caf::flow::op
