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

template <class T, class Predicate>
class retry_sub : public subscription::impl_base, public observer_impl<T> {
public:
  // -- constructors, destructors, and assignment operators --------------------

  retry_sub(coordinator* parent, observer<T> out, Predicate predicate,
            observable<T> in)
    : parent_(parent),
      out_(std::move(out)),
      in_(std::move(in)),
      predicate_(std::move(predicate)) {
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
    if (sub_)
      sub_.request(new_demand);
  }

  // -- implementation of observer_impl ----------------------------------------

  void ref_coordinated() const noexcept override {
    ref();
  }

  void deref_coordinated() const noexcept override {
    deref();
  }

  void on_subscribe(subscription sub) override {
    if (sub_) {
      sub_.cancel();
      sub_.release_later();
    }
    sub_ = std::move(sub);
    if (demand_ > 0)
      sub_.request(demand_);
  }

  void on_next(const T& item) override {
    if (!out_)
      return;
    if (demand_ > 0) {
      --demand_;
      out_.on_next(item);
    }
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
    sub_.release_later();
    if (predicate_(what))
      parent()->delay_fn([this]() { in_.subscribe(this->as_observer()); });
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

  size_t demand_ = 0u;

  /// Stores the context (coordinator) that runs this flow.
  coordinator* parent_;

  /// Stores a handle to the subscribed observer.
  observer<T> out_;

  /// Stores a handle to the subscribed observable.
  observable<T> in_;

  /// Stores the subscription to the input observable.
  subscription sub_;

  /// Stores the number of retries remaining.
  Predicate predicate_;
};

/// An observable that retry calls any callbacks on its
/// subscribers.
template <class T, class Predicate>
class retry : public hot<T> {
public:
  using trait = detail::get_callable_trait_t<Predicate>;

  using arg_type
    = std::decay_t<caf::detail::tl_head_t<typename trait::arg_types>>;

  static_assert(std::is_convertible_v<typename trait::result_type, bool>,
                "predicates must return a boolean value");

  static_assert(trait::num_args == 1,
                "predicates must take exactly one argument");

  static_assert(std::is_same_v<arg_type, caf::error>,
                "predicates must take only error as argument");

  // -- member types -----------------------------------------------------------

  using super = hot<T>;

  // -- constructors, destructors, and assignment operators --------------------

  retry(coordinator* parent, observable<T> in, Predicate predicate)
    : super(parent), in_(std::move(in)), predicate_(std::move(predicate)) {
    // nop
  }

  // -- implementation of observable_impl<T> -----------------------------------

  disposable subscribe(observer<T> out) override {
    CAF_ASSERT(out.valid());
    using sub_t = retry_sub<T, Predicate>;
    auto ptr = super::parent_->add_child(std::in_place_type<sub_t>, out,
                                         predicate_, in_);
    out.on_subscribe(subscription{ptr});
    in_.subscribe(ptr->as_observer());
    return disposable{ptr->as_disposable()};
  }

private:
  observable<T> in_;

  Predicate predicate_;
};

} // namespace caf::flow::op
