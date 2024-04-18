// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/config.hpp"
#include "caf/detail/assert.hpp"
#include "caf/flow/backpressure_overflow_strategy.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/op/hot.hpp"
#include "caf/flow/subscription.hpp"

#include <deque>
#include <utility>

namespace caf::flow::op {

template <class T>
class retry_sub : public subscription::impl_base, public observer_impl<T> {
public:
  // -- constructors, destructors, and assignment operators --------------------

  retry_sub(coordinator* parent, observer<T> out, uint32_t retry_remaining,
            observable<T> in)
    : parent_(parent),
      out_(std::move(out)),
      retry_remaining_(retry_remaining),
      in_(std::move(in)) {
    // nop
  }

  // -- implementation of subscription -----------------------------------------

  coordinator* parent() const noexcept override {
    return parent_;
  }

  bool disposed() const noexcept override {
    return !out_;
  }

  void request(size_t new_demand) override {
    if (new_demand == 0)
      return;
    demand_ += new_demand;
  }

  // -- implementation of observer_impl ----------------------------------------

  void ref_coordinated() const noexcept override {
    ref();
  }

  void deref_coordinated() const noexcept override {
    deref();
  }

  void on_subscribe(subscription sub) override {
    if (sub_ && retry_pending()) {
      sub_.cancel();
      sub_.release_later();
      --retry_remaining_;
    } else if (sub_ && !retry_pending()) {
      sub_.cancel();
      return;
    }
    sub_ = std::move(sub);
    sub_.request(256);
  }

  void on_next(const T& item) override {
    if (!out_)
      return;
    if (demand_ > 0) {
      --demand_;
      out_.on_next(item);
      if (sub_)
        sub_.request(1);
      return;
    }
    sub_.request(1);
  }

  void on_complete() override {
    if (!out_)
      return;
    sub_.release_later();
    out_.on_complete();
  }

  void on_error(const error& what) override {
    if (!out_)
      return;
    if (retry_pending())
      in_.subscribe(this->as_observer());
    else
      out_.on_error(what);
  }

private:
  void do_dispose(bool from_external) override {
    if (!out_)
      return;
    sub_.cancel();
    if (from_external)
      out_.on_error(make_error(sec::disposed));
  }

  bool retry_pending() {
    return retry_remaining_ > 0;
  }

  size_t demand_;

  /// Stores the context (coordinator) that runs this flow.
  coordinator* parent_;

  /// Stores a handle to the subscribed observer.
  observer<T> out_;

  /// Stores a handle to the subscribed observable.
  observable<T> in_;

  /// Stores the subscription to the input observable.
  subscription sub_;

  /// Stores the number of retries remaining.
  uint32_t retry_remaining_;
};

/// An observable that retry calls any callbacks on its
/// subscribers.
template <class T>
class retry : public hot<T> {
public:
  // -- member types -----------------------------------------------------------

  using super = hot<T>;

  // -- constructors, destructors, and assignment operators --------------------

  retry(coordinator* parent, observable<T> in, uint32_t retry_limit)
    : super(parent), in_(std::move(in)), retry_limit_(retry_limit) {
    // nop
  }

  // -- implementation of observable_impl<T> -----------------------------------

  disposable subscribe(observer<T> out) override {
    CAF_ASSERT(out.valid());
    using sub_t = retry_sub<T>;
    auto ptr = super::parent_->add_child(std::in_place_type<sub_t>, out,
                                         retry_limit_, in_);
    out.on_subscribe(subscription{ptr});
    in_.subscribe(ptr->as_observer());
    return disposable{ptr->as_disposable()};
  }

private:
  observable<T> in_;

  uint32_t retry_limit_;
};

} // namespace caf::flow::op
