// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/config.hpp"
#include "caf/detail/assert.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/op/hot.hpp"
#include "caf/flow/subscription.hpp"

#include <type_traits>
#include <utility>

namespace caf::flow::op {

template <class T, class Predicate>
class on_error_resume_next_sub : public subscription::impl_base,
                                 public observer_impl<T> {
public:
  // -- constructors, destructors, and assignment operators --------------------

  on_error_resume_next_sub(coordinator* parent, observer<T> out,
                           Predicate predicate, observable<T> fallback)
    : parent_(parent),
      out_(std::move(out)),
      fallback_(std::move(fallback)),
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
      sub.cancel();
      return;
    }
    sub_ = std::move(sub);
    if (demand_ > 0)
      sub_.request(demand_);
  }

  void on_next(const T& item) override {
    if (!out_ || demand_ == 0)
      return;
    --demand_;
    out_.on_next(item);
  }

  void on_complete() override {
    if (!out_)
      return;
    sub_.release_later();
    out_.on_complete();
    fallback_ = nullptr;
  }

  void on_error(const error& what) override {
    if (!out_)
      return;
    sub_.release_later();
    if (fallback_ && predicate_(what)) {
      parent_->delay_fn(
        [sptr = intrusive_ptr{this}, fallback = std::move(fallback_)] {
          sptr->do_resume_next(fallback);
        });
    } else {
      out_.on_error(what);
      fallback_ = nullptr;
    }
  }

private:
  void do_dispose(bool from_external) override {
    if (!out_)
      return;
    if (from_external) {
      out_.on_error(make_error(sec::disposed));
    } else {
      out_.release_later();
    }
    sub_.cancel();
    fallback_ = nullptr;
  }

  void do_resume_next(observable<T> fallback) {
    if (!out_ || !fallback)
      return;
    fallback.subscribe(this->as_observer());
  }

  // Stores the pending demand. When re-subscribing, we transfer the demand to
  // the new subscription.
  size_t demand_ = 0u;

  /// Stores the context (coordinator) that runs this flow.
  coordinator* parent_;

  /// Stores a handle to the subscribed observer.
  observer<T> out_;

  /// Stores a handle to the fallback observable.
  observable<T> fallback_;

  /// Stores the subscription to the input observable.
  subscription sub_;

  /// Stores the number of retries remaining.
  Predicate predicate_;
};

/// Operator for switching to a secondary input source if the primary input
/// fails.
template <class T, class Predicate>
class on_error_resume_next : public hot<T> {
public:
  // -- static assertions ------------------------------------------------------

  static_assert(std::is_invocable_r_v<bool, Predicate, const caf::error&>,
                "Predicate must have the signature bool(const caf::error&)");

  // -- member types -----------------------------------------------------------

  using super = hot<T>;

  // -- constructors, destructors, and assignment operators --------------------

  on_error_resume_next(coordinator* parent, observable<T> in,
                       Predicate predicate, observable<T> fallback)
    : super(parent),
      in_(std::move(in)),
      predicate_(std::move(predicate)),
      fallback_(std::move(fallback)) {
    // nop
  }

  // -- implementation of observable_impl<T> -----------------------------------

  disposable subscribe(observer<T> out) override {
    CAF_ASSERT(out.valid());
    using sub_t = on_error_resume_next_sub<T, Predicate>;
    auto ptr = super::parent_->add_child(std::in_place_type<sub_t>, out,
                                         predicate_, fallback_);
    out.on_subscribe(subscription{ptr});
    in_.subscribe(ptr->as_observer());
    return disposable{ptr->as_disposable()};
  }

private:
  /// Stores the decorated observable.
  observable<T> in_;

  /// Stores the predicate that determines whether to run fallback observable
  Predicate predicate_;

  /// Stores the fallback observable.
  observable<T> fallback_;
};

} // namespace caf::flow::op
