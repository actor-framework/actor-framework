// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/flow/observer.hpp"
#include "caf/flow/op/cold.hpp"
#include "caf/flow/op/empty.hpp"
#include "caf/flow/subscription.hpp"
#include "caf/make_counted.hpp"
#include "caf/unordered_flat_map.hpp"

#include <deque>
#include <memory>
#include <numeric>
#include <utility>
#include <variant>
#include <vector>

namespace caf::flow::op {

/// Receives observables from the pre-merge step and merges their inputs for the
/// observer.
template <class T>
class merge_sub : public subscription::impl_base,
                  public observer_impl<observable<T>> {
public:
  // -- member types -----------------------------------------------------------

  using input_key = size_t;

  using input_map = unordered_flat_map<input_key, subscription>;

  struct item_t {
    T value;
    input_key source;
  };

  using item_queue = std::deque<item_t>;

  // -- constants --------------------------------------------------------------

  /// Limits how many items the merge operator pulls in per input. This is
  /// deliberately small to make sure that we get reasonably small "batches" of
  /// items per input to make sure all inputs get their turn.
  static constexpr size_t default_max_pending_per_input = 8;

  // -- constructors, destructors, and assignment operators --------------------

  merge_sub(coordinator* ctx, observer<T> out, size_t max_concurrent,
            size_t max_pending_per_input = default_max_pending_per_input)
    : ctx_(ctx),
      out_(std::move(out)),
      max_concurrent_(max_concurrent),
      max_pending_per_input_(max_pending_per_input) {
    // nop
  }

  // -- implementation of observer_impl ----------------------------------------

  void ref_coordinated() const noexcept override {
    ref();
  }

  void deref_coordinated() const noexcept override {
    deref();
  }

  void on_next(const observable<T>& what) override {
    CAF_ASSERT(what);
    auto key = next_key_++;
    inputs_.emplace(key, subscription{});
    using fwd_impl = forwarder<T, merge_sub, size_t>;
    auto fwd = make_counted<fwd_impl>(this, key);
    what.pimpl()->subscribe(fwd->as_observer());
  }

  void on_error(const error& what) override {
    sub_ = nullptr;
    if (out_) {
      if (inputs_.empty() && queue_.empty()) {
        auto out = std::move(out_);
        out.on_error(what);
        return;
      }
      err_ = what;
    }
  }

  void on_complete() override {
    sub_ = nullptr;
    if (out_ && inputs_.empty() && queue_.empty()) {
      auto out = std::move(out_);
      out.on_complete();
    }
  }

  void on_subscribe(flow::subscription sub) override {
    if (!sub_ && out_) {
      sub_ = std::move(sub);
      if (max_concurrent_ > inputs_.size()) {
        // Note: the factory might call on_next a couple of times before
        // subscribing this object to the pre-merge.
        auto new_demand = max_concurrent_ - inputs_.size();
        sub_.request(new_demand);
      }
    } else {
      sub.dispose();
    }
  }

  friend void intrusive_ptr_add_ref(const merge_sub* ptr) noexcept {
    ptr->ref();
  }

  friend void intrusive_ptr_release(const merge_sub* ptr) noexcept {
    ptr->deref();
  }

  // -- callbacks for the forwarders -------------------------------------------

  void fwd_on_subscribe(input_key key, subscription sub) {
    if (auto ptr = get(key); ptr && !*ptr) {
      *ptr = std::move(sub);
      ptr->request(max_pending_per_input_);
    } else {
      sub.dispose();
    }
  }

  void fwd_on_complete(input_key key) {
    if (inputs_.erase(key) == 0)
      return;
    if (sub_) {
      if (inputs_.size() < max_concurrent_)
        sub_.request(1);
      return;
    }
    if (inputs_.empty() && queue_.empty()) {
      auto out = std::move(out_);
      out.on_complete();
    }
  }

  void fwd_on_error(input_key key, const error& what) {
    if (inputs_.erase(key) == 0)
      return;
    err_ = what;
    drop_inputs();
    if (queue_.empty()) {
      auto out = std::move(out_);
      out.on_error(what);
    }
  }

  void fwd_on_next(input_key key, const T& item) {
    if (auto ptr = get(key)) {
      if (!running_ && demand_ > 0) {
        CAF_ASSERT(out_.valid());
        --demand_;
        if (*ptr)
          ptr->request(1);
        out_.on_next(item);
      } else {
        queue_.push_back(item_t{item, key});
      }
    }
  }

  // -- implementation of subscription_impl ------------------------------------

  bool disposed() const noexcept override {
    return !out_;
  }

  void dispose() override {
    if (out_) {
      drop_inputs();
      queue_.clear();
      ctx_->delay_fn([out = std::move(out_)]() mutable { out.on_complete(); });
      if (sub_)
        sub_.dispose();
    }
  }

  void request(size_t n) override {
    if (out_) {
      demand_ += n;
      if (demand_ == n)
        run_later();
    }
  }

  size_t buffered() const noexcept {
    return queue_.size();
  }

private:
  void drop_inputs() {
    input_map inputs;
    inputs.swap(inputs_);
    for (auto& [key, sub] : inputs)
      sub.dispose();
  }

  void run_later() {
    if (!running_) {
      running_ = true;
      ctx_->delay_fn([strong_this = intrusive_ptr<merge_sub>{this}] {
        strong_this->do_run();
      });
    }
  }

  bool done() const noexcept {
    return !sub_ && inputs_.empty() && queue_.empty();
  }

  void do_run() {
    while (out_ && demand_ > 0 && !queue_.empty()) {
      // Fetch the next item.
      auto [item, key] = std::move(queue_.front());
      queue_.pop_front();
      --demand_;
      // Request a new item from the input if we still have a subscription.
      if (auto ptr = get(key); ptr && *ptr)
        ptr->request(1);
      // Call the observer. This might nuke out_ by calling dispose().
      out_.on_next(item);
    }
    running_ = false;
    // Check if we can call it a day.
    if (out_ && done()) {
      auto out = std::move(out_);
      if (!err_)
        out.on_complete();
      else
        out.on_error(err_);
    }
  }

  /// Selects an input object by key or returns null.
  subscription* get(input_key key) {
    if (auto i = inputs_.find(key); i != inputs_.end())
      return std::addressof(i->second);
    else
      return nullptr;
  }

  /// Stores the context (coordinator) that runs this flow.
  coordinator* ctx_;

  /// Stores the first error that occurred on any input.
  error err_;

  /// Subscription to the pre-merger that produces the input observables.
  subscription sub_;

  /// Stores whether the merge is currently executing do_run.
  bool running_ = false;

  /// Stores our current demand for items from the subscriber.
  size_t demand_ = 0;

  /// Stores a handle to the subscriber.
  observer<T> out_;

  /// Stores the last round-robin read position.
  size_t pos_ = 0;

  /// Associates inputs with ascending keys.
  input_map inputs_;

  /// Caches items that arrived without having downstream demand.
  item_queue queue_;

  /// Stores the key for the next input.
  size_t next_key_ = 1;

  /// Configures how many items we buffer per input.
  size_t max_concurrent_;

  /// Configures how many items we have pending per input at most.
  size_t max_pending_per_input_;
};

template <class T>
class merge : public cold<T> {
public:
  // -- member types -----------------------------------------------------------

  using super = cold<T>;

  // -- constructors, destructors, and assignment operators --------------------

  template <class... Ts, class... Inputs>
  explicit merge(coordinator* ctx, Inputs&&... inputs) : super(ctx) {
    (add(std::forward<Inputs>(inputs)), ...);
  }

  // -- properties -------------------------------------------------------------

  size_t inputs() const noexcept {
    return plain_inputs_.size() + wrapped_inputs_.size();
  }

  // -- implementation of observable_impl<T> -----------------------------------

  disposable subscribe(observer<T> out) override {
    using sub_t = merge_sub<T>;
    using pre_sub_t = merge_sub<observable<T>>;
    // Trivial case: nothing to do.
    if (inputs() == 0) {
      auto ptr = make_counted<empty<T>>(super::ctx_);
      return ptr->subscribe(std::move(out));
    }
    // Simple case: all observables for the merge are available right away.
    if (wrapped_inputs_.empty()) {
      auto sub = make_counted<merge_sub<T>>(super::ctx_, out, max_concurrent_);
      for (auto& input : plain_inputs_)
        sub->on_next(input);
      out.on_subscribe(subscription{sub});
      return sub->as_disposable();
    }
    // Complex case: we need a "pre-merge" step to get the observables for the
    // actual merge operation.
    auto sub = make_counted<sub_t>(super::ctx_, out, max_concurrent_);
    for (auto& input : plain_inputs_)
      sub->on_next(input);
    auto pre_sub = make_counted<pre_sub_t>(super::ctx(), sub->as_observer(),
                                           max_concurrent_, 1);
    for (auto& input : wrapped_inputs_)
      pre_sub->on_next(input);
    sub->on_subscribe(subscription{pre_sub});
    out.on_subscribe(subscription{sub});
    return sub->as_disposable();
  }

private:
  void do_add(observable<observable<T>> in) {
    wrapped_inputs_.emplace_back(std::move(in));
  }

  void do_add(observable<T> in) {
    plain_inputs_.emplace_back(std::move(in));
  }

  template <class Input>
  void add(Input&& x) {
    using input_t = std::decay_t<Input>;
    if constexpr (detail::is_iterable_v<input_t>) {
      for (auto& in : x)
        add(in);
    } else {
      static_assert(is_observable_v<input_t>);
      do_add(std::forward<Input>(x).as_observable());
    }
  }

  std::vector<observable<T>> plain_inputs_;
  std::vector<observable<observable<T>>> wrapped_inputs_;
  size_t max_concurrent_ = defaults::flow::max_concurrent;
};

} // namespace caf::flow::op
