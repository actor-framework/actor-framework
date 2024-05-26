// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/assert.hpp"
#include "caf/flow/gen/from_container.hpp"
#include "caf/flow/gen/just.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/op/cold.hpp"
#include "caf/flow/op/empty.hpp"
#include "caf/flow/op/from_generator.hpp"
#include "caf/flow/op/pullable.hpp"
#include "caf/flow/subscription.hpp"

#include <deque>
#include <map>
#include <memory>
#include <numeric>
#include <utility>
#include <vector>

namespace caf::flow::op {

template <class T>
struct merge_input {
  using value_type = T;

  subscription sub;
  std::deque<T> buf;

  T pop() {
    auto result = std::move(buf.front());
    buf.pop_front();
    return result;
  }

  /// Returns whether the input can no longer produce additional items.
  bool at_end() const noexcept {
    return !sub && buf.empty();
  }
};

/// Receives observables from the pre-merge step and merges their inputs for the
/// observer.
template <class T>
class merge_sub : public subscription::impl_base,
                  public observer_impl<observable<T>>,
                  public pullable {
public:
  // -- member types -----------------------------------------------------------

  using input_key = size_t;

  using input_map = std::map<input_key, merge_input<T>>;

  using input_map_iterator = typename input_map::iterator;

  // -- constants --------------------------------------------------------------

  /// Limits how many items the merge operator pulls in per input. This is
  /// deliberately small to make sure that we get reasonably small "batches" of
  /// items per input to make sure all inputs get their turn.
  static constexpr size_t default_max_pending_per_input = 8;

  // -- constructors, destructors, and assignment operators --------------------

  merge_sub(coordinator* parent, observer<T> out, size_t max_concurrent,
            size_t max_pending_per_input = default_max_pending_per_input)
    : parent_(parent),
      out_(std::move(out)),
      max_concurrent_(max_concurrent),
      max_pending_per_input_(max_pending_per_input) {
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
    auto key = next_key_++;
    inputs_.emplace(key, merge_input<T>{});
    using fwd_impl = forwarder<T, merge_sub, size_t>;
    auto fwd = parent_->add_child(std::in_place_type<fwd_impl>, this, key);
    what.pimpl()->subscribe(fwd->as_observer());
  }

  void on_error(const error& what) override {
    sub_.release_later();
    err_ = what;
    stop_inputs();
    if (out_ && inputs_.empty())
      out_.on_error(what);
  }

  void on_complete() override {
    sub_.release_later();
    if (out_ && inputs_.empty() && buffered_ == 0)
      out_.on_complete();
  }

  void on_subscribe(flow::subscription sub) override {
    if (!sub_ && out_) {
      sub_ = std::move(sub);
      sub_.request(max_concurrent_);
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

  friend void intrusive_ptr_add_ref(const merge_sub* ptr) noexcept {
    ptr->ref();
  }

  friend void intrusive_ptr_release(const merge_sub* ptr) noexcept {
    ptr->deref();
  }

  // -- callbacks for the forwarders -------------------------------------------

  void fwd_on_subscribe(input_key key, subscription sub) {
    auto lg = log::core::trace("key = {}", key);
    if (auto ptr = get(key); ptr && !ptr->sub) {
      ptr->sub = std::move(sub);
      ptr->sub.request(max_pending_per_input_);
    } else {
      sub.cancel();
    }
  }

  void fwd_on_complete(input_key key) {
    auto lg = log::core::trace("key = {}", key);
    auto i = inputs_.find(key);
    if (i == inputs_.end())
      return;
    if (!i->second.buf.empty()) {
      i->second.sub.release_later();
      return;
    }
    inputs_.erase(i);
    if (sub_) {
      sub_.request(1);
      return;
    }
    if (inputs_.empty())
      out_.on_complete();
  }

  void fwd_on_error(input_key key, const error& what) {
    auto lg = log::core::trace("key = {}, what = {}", key, what);
    if (err_)
      return;
    auto i = inputs_.find(key);
    if (i == inputs_.end())
      return;
    err_ = what;
    stop_inputs();
    if (inputs_.empty())
      out_.on_error(what);
  }

  void fwd_on_next(input_key key, const T& item) {
    auto lg = log::core::trace("key = {}, item = {}", key, item);
    if (auto ptr = get(key)) {
      if (!this->is_pulling() && demand_ > 0) {
        CAF_ASSERT(out_.valid());
        --demand_;
        if (ptr->sub)
          ptr->sub.request(1);
        out_.on_next(item);
      } else {
        ++buffered_;
        ptr->buf.push_back(item);
      }
    }
  }

  // -- implementation of subscription_impl ------------------------------------

  bool disposed() const noexcept override {
    return !out_;
  }

  void request(size_t n) override {
    if (!out_)
      return;
    if (buffered_ == 0)
      demand_ += n;
    else
      this->pull(parent_, n);
  }

  // -- properties -------------------------------------------------------------

  size_t buffered() const noexcept {
    return buffered_;
  }

  size_t demand() const noexcept {
    return demand_;
  }

  size_t num_inputs() const noexcept {
    return inputs_.size();
  }

  bool subscribed() const noexcept {
    return sub_.valid();
  }

  size_t max_concurrent() const noexcept {
    return max_concurrent_;
  }

private:
  // -- implementation of subscription::impl_base ------------------------------

  void do_dispose(bool from_external) override {
    if (!out_)
      return;
    input_map xs;
    xs.swap(inputs_);
    for (auto& kvp : xs)
      kvp.second.sub.cancel();
    sub_.cancel();
    if (from_external)
      out_.on_error(make_error(sec::disposed));
    else
      out_.release_later();
  }

  // -- implementation of pullable ---------------------------------------------

  void do_pull(size_t n) override {
    demand_ += n;
    auto i = next();
    if (i == inputs_.end())
      return;
    auto old_pos = pos_;
    while (out_ && demand_ > 0 && buffered_ > 0) {
      // Fetch the next item.
      auto item = i->second.pop();
      --demand_;
      --buffered_;
      // Request a new item from the input if we still have a subscription.
      // Otherwise, drop this input if we have exhausted all items.
      if (i->second.sub) {
        i->second.sub.request(1);
        if (i->second.buf.empty() && buffered_ > 0)
          i = next();
      } else if (i->second.buf.empty()) {
        i = inputs_.erase(i);
        if (sub_)
          sub_.request(1);
        if (buffered_ > 0)
          i = next();
      }
      // Call the observer. This might nuke out_ by calling dispose().
      out_.on_next(item);
    }
    // Make sure we don't get stuck on a single input.
    if (pos_ == old_pos)
      ++pos_;
    // Check if we can call it a day.
    if (out_ && done()) {
      if (!err_)
        out_.on_complete();
      else
        out_.on_error(err_);
    }
  }

  void do_ref() override {
    this->ref();
  }

  void do_deref() override {
    this->deref();
  }

  void stop_inputs() {
    auto i = inputs_.begin();
    while (i != inputs_.end()) {
      i->second.sub.cancel();
      if (i->second.buf.empty())
        i = inputs_.erase(i);
      else
        ++i;
    }
    sub_.cancel();
  }

  bool done() const noexcept {
    return !sub_ && inputs_.empty();
  }

  /// Selects an input subscription by key or returns null.
  merge_input<T>* get(input_key key) {
    if (auto i = inputs_.find(key); i != inputs_.end())
      return std::addressof(i->second);
    else
      return nullptr;
  }

  input_map_iterator select_non_empty(input_map_iterator first,
                                      input_map_iterator last) {
    auto non_empty = [](const auto& kvp) { return !kvp.second.buf.empty(); };
    return std::find_if(first, last, non_empty);
  }

  /// Selects an input subscription by key or returns null.
  input_map_iterator next() {
    if (inputs_.empty())
      return inputs_.end();
    input_map_iterator result;
    if (auto i = inputs_.lower_bound(pos_); i != inputs_.end()) {
      result = select_non_empty(i, inputs_.end());
      if (result == inputs_.end())
        result = select_non_empty(inputs_.begin(), i);
    } else {
      result = select_non_empty(inputs_.begin(), inputs_.end());
    }
    if (result != inputs_.end())
      pos_ = result->first;
    return result;
  }

  /// Stores the context (coordinator) that runs this flow.
  coordinator* parent_;

  /// Stores the first error that occurred on any input.
  error err_;

  /// Subscription to the pre-merger that produces the input observables.
  subscription sub_;

  /// Stores our current demand for items from the subscriber.
  size_t demand_ = 0;

  /// Stores a handle to the subscriber.
  observer<T> out_;

  /// Associates inputs with ascending keys.
  input_map inputs_;

  /// Stores the key for the next input.
  size_t next_key_ = 1;

  /// Stores the key for the next item.
  size_t pos_ = 1;

  /// Stores how many items are buffered in total.
  size_t buffered_ = 0;

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

  template <class... Inputs>
  merge(coordinator* parent, observable<T> input0, observable<T> input1,
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

  merge(coordinator* parent, observable<observable<T>> inputs)
    : super(parent), inputs_(std::move(inputs)) {
    // nop
  }

  // -- implementation of observable_impl<T> -----------------------------------

  disposable subscribe(observer<T> out) override {
    auto sub = super::parent_->add_child(std::in_place_type<merge_sub<T>>, out,
                                         max_concurrent_);
    inputs_.subscribe(sub->as_observer());
    out.on_subscribe(subscription{sub});
    return sub->as_disposable();
  }

private:
  observable<observable<T>> inputs_;

  size_t max_concurrent_ = defaults::flow::max_concurrent;
};

} // namespace caf::flow::op
