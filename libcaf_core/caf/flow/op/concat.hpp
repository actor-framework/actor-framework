// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/flow/gen/from_container.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/op/cold.hpp"
#include "caf/flow/op/empty.hpp"
#include "caf/flow/op/from_generator.hpp"
#include "caf/flow/subscription.hpp"

#include <deque>
#include <memory>
#include <numeric>
#include <utility>
#include <variant>
#include <vector>

namespace caf::flow::op {

/// Combines items from any number of observables.
template <class T>
class concat_sub : public subscription::impl_base,
                   public observer_impl<observable<T>> {
public:
  // -- member types -----------------------------------------------------------

  using input_key = size_t;

  using input_type = observable<observable<T>>;

  // -- constructors, destructors, and assignment operators --------------------

  concat_sub(coordinator* parent, observer<T> out)
    : parent_(parent), out_(out) {
    // nop
  }

  // -- implementation of observer_impl ----------------------------------------

  coordinator* parent() const noexcept override {
    return parent_;
  }

  void on_next(const observable<T>& what) override {
    CAF_ASSERT(what);
    if (!sub_)
      return;
    ++key_;
    using fwd_impl = forwarder<T, concat_sub, size_t>;
    auto fwd = parent_->add_child(std::in_place_type<fwd_impl>, this, key_);
    what.pimpl()->subscribe(fwd->as_observer());
  }

  void on_error(const error& what) override {
    sub_.release_later();
    err_ = what;
    if (!fwd_sub_ && out_) {
      ++key_; // Increment the key to ignore any events from the last forwarder.
      out_.on_error(err_);
    }
  }

  void on_complete() override {
    sub_.release_later();
    if (!fwd_sub_ && out_) {
      ++key_; // Increment the key to ignore any events from the last forwarder.
      out_.on_complete();
    }
  }

  void on_subscribe(flow::subscription sub) override {
    if (!sub_ && out_) {
      sub_ = std::move(sub);
      sub_.request(1);
    } else {
      sub.cancel();
    }
  }

  // -- reference counting -----------------------------------------------------

  void ref_coordinated() const noexcept final {
    ref();
  }

  void deref_coordinated() const noexcept final {
    deref();
  }

  friend void intrusive_ptr_add_ref(const concat_sub* ptr) noexcept {
    ptr->ref();
  }

  friend void intrusive_ptr_release(const concat_sub* ptr) noexcept {
    ptr->deref();
  }

  // -- callbacks for the forwarders -------------------------------------------

  void fwd_on_subscribe(input_key key, subscription sub) {
    if (key != key_ || fwd_sub_) {
      sub.cancel();
      return;
    }
    fwd_sub_ = std::move(sub);
    if (in_flight_ > 0)
      fwd_sub_.request(in_flight_);
  }

  void fwd_on_complete(input_key key) {
    if (key != key_)
      return;
    fwd_sub_.release_later();
    // Fetch next observable if the source is still alive.
    if (sub_) {
      sub_.request(1);
      return;
    }
    // Otherwise, we're done.
    ++key_;
    if (!err_)
      out_.on_complete();
    else
      out_.on_error(err_);
  }

  void fwd_on_error(input_key key, const error& what) {
    if (key != key_)
      return;
    ++key_;
    sub_.cancel();
    fwd_sub_.release_later();
    out_.on_error(what);
  }

  void fwd_on_next(input_key key, const T& item) {
    if (key != key_)
      return;
    CAF_ASSERT(in_flight_ > 0);
    --in_flight_;
    out_.on_next(item);
  }

  // -- implementation of subscription -----------------------------------------

  bool disposed() const noexcept override {
    return !out_;
  }

  void request(size_t n) override {
    CAF_ASSERT(out_.valid());
    if (fwd_sub_)
      fwd_sub_.request(n);
    in_flight_ += n;
  }

private:
  void do_dispose(bool from_external) override {
    if (!out_)
      return;
    ++key_;
    sub_.cancel();
    fwd_sub_.cancel();
    if (from_external)
      out_.on_complete();
    else
      out_.release_later();
  }

  /// Stores the context (coordinator) that runs this flow.
  coordinator* parent_;

  /// Stores a handle to the subscribed observer.
  observer<T> out_;

  /// Subscription to the operator that emits the obbservables to concatenate.
  subscription sub_;

  /// Our currently active subscription.
  subscription fwd_sub_;

  /// Caches the error of the input source to delay the error until the current
  /// observable completes.
  error err_;

  /// Stores the key for the current observable. The operator will discard any
  /// data from previous observables.
  input_key key_ = 0;

  /// Stores how much demand we have left. When switching to a new input, we
  /// pass any demand unused by the previous input to the new one.
  size_t in_flight_ = 0;
};

template <class T>
class concat : public cold<T> {
public:
  // -- member types -----------------------------------------------------------

  using super = cold<T>;

  using input_type = std::variant<observable<T>, observable<observable<T>>>;

  // -- constructors, destructors, and assignment operators --------------------

  template <class... Inputs>
  concat(coordinator* parent, observable<T> input0, observable<T> input1,
         Inputs... inputs)
    : super(parent) {
    static_assert((std::is_same_v<observable<T>, Inputs> && ...));
    using vector_t = std::vector<observable<T>>;
    using gen_t = gen::from_container<vector_t>;
    using obs_t = from_generator<gen::from_container<vector_t>>;
    auto xs = vector_t{};
    xs.reserve(sizeof...(Inputs) + 2);
    xs.emplace_back(std::move(input0));
    xs.emplace_back(std::move(input1));
    (xs.emplace_back(std::move(inputs)), ...);
    inputs_ = super::parent_->add_child(std::in_place_type<obs_t>,
                                        gen_t{std::move(xs)}, std::tuple{});
  }

  concat(coordinator* parent, observable<observable<T>> inputs)
    : super(parent), inputs_(std::move(inputs)) {
    // nop
  }

  // -- implementation of observable<T> -----------------------------------

  disposable subscribe(observer<T> out) override {
    auto sub = super::parent_->add_child(std::in_place_type<concat_sub<T>>,
                                         out);
    inputs_.subscribe(sub->as_observer());
    out.on_subscribe(subscription{sub});
    return sub->as_disposable();
  }

private:
  observable<observable<T>> inputs_;
};

} // namespace caf::flow::op
