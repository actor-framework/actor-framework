// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/config.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/op/cold.hpp"
#include "caf/flow/subscription.hpp"

#include <utility>

namespace caf::flow::op {

template <class T>
class never_sub : public subscription::impl_base {
public:
  // -- constructors, destructors, and assignment operators --------------------

  never_sub(coordinator* parent, observer<T> out)
    : parent_(parent), out_(std::move(out)) {
    // nop
  }

  // -- implementation of subscription -----------------------------------------

  coordinator* parent() const noexcept override {
    return parent_;
  }

  bool disposed() const noexcept override {
    return !out_;
  }

  void dispose() override {
    if (out_)
      parent_->delay_fn([out = std::move(out_)]() mutable { //
        out.on_complete();
      });
  }

  void request(size_t) override {
    // nop
  }

private:
  /// Stores the context (coordinator) that runs this flow.
  coordinator* parent_;

  /// Stores a handle to the subscribed observer.
  observer<T> out_;
};

/// An observable that never calls any callbacks on its subscribers.
template <class T>
class never : public cold<T> {
public:
  // -- member types -----------------------------------------------------------

  using super = cold<T>;

  // -- constructors, destructors, and assignment operators --------------------

  explicit never(coordinator* parent) : super(parent) {
    // nop
  }

  // -- implementation of observable_impl<T> -----------------------------------

  disposable subscribe(observer<T> out) override {
    CAF_ASSERT(out.valid());
    auto sub = super::parent_->add_child_hdl(std::in_place_type<never_sub<T>>,
                                             out);
    out.on_subscribe(sub);
    return disposable{std::move(sub).as_disposable()};
  }
};

} // namespace caf::flow::op
