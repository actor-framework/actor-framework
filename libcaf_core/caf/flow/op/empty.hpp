// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/flow/observable.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/op/cold.hpp"
#include "caf/flow/subscription.hpp"

#include <cstdint>

namespace caf::flow::op {

template <class T>
class empty_sub : public subscription::impl_base {
public:
  // -- constructors, destructors, and assignment operators --------------------

  empty_sub(coordinator* ctx, observer<T> out)
    : ctx_(ctx), out_(std::move(out)) {
    // nop
  }

  // -- implementation of subscription -----------------------------------------

  bool disposed() const noexcept override {
    return !out_;
  }

  void cancel() override {
    if (out_)
      ctx_->delay_fn([out = std::move(out_)]() mutable { out.on_complete(); });
  }

  void request(size_t) override {
    cancel();
  }

private:
  /// Stores the context (coordinator) that runs this flow.
  coordinator* ctx_;

  /// Stores a handle to the subscribed observer.
  observer<T> out_;
};

/// An observable that represents an empty range. As soon as an observer
/// requests values from this observable, it calls `on_complete`.
template <class T>
class empty : public cold<T> {
public:
  // -- member types -----------------------------------------------------------

  using super = cold<T>;

  using output_type = T;

  // -- constructors, destructors, and assignment operators --------------------

  explicit empty(coordinator* ctx) : super(ctx) {
    // nop
  }

  // -- implementation of observable<T>::impl ----------------------------------

  disposable subscribe(observer<output_type> out) override {
    auto ptr = make_counted<empty_sub<T>>(super::ctx_, out);
    out.on_subscribe(subscription{ptr});
    return disposable{std::move(ptr)};
  }
};

} // namespace caf::flow::op
