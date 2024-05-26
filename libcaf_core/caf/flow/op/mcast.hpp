// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/flow/coordinator.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/op/empty.hpp"
#include "caf/flow/op/hot.hpp"
#include "caf/flow/op/ucast.hpp"
#include "caf/flow/subscription.hpp"
#include "caf/intrusive_ptr.hpp"

#include <algorithm>
#include <deque>
#include <memory>
#include <numeric>

namespace caf::flow::op {

/// State shared between one multicast operator and one subscribed observer.
template <class T>
using mcast_sub_state = ucast_sub_state<T>;

template <class T>
using mcast_sub_state_ptr = intrusive_ptr<mcast_sub_state<T>>;

template <class T>
class mcast_sub : public subscription::impl_base {
public:
  // -- constructors, destructors, and assignment operators --------------------

  mcast_sub(coordinator* parent, mcast_sub_state_ptr<T> state)
    : parent_(parent), state_(std::move(state)) {
    // nop
  }

  // -- implementation of subscription -----------------------------------------

  coordinator* parent() const noexcept override {
    return parent_;
  }

  bool disposed() const noexcept override {
    return !state_ || state_->disposed;
  }

  void request(size_t n) override {
    state_->request(n);
  }

private:
  void do_dispose(bool) override {
    if (state_) {
      auto state = std::move(state_);
      state->dispose();
    }
  }

  /// Stores the context (coordinator) that runs this flow.
  coordinator* parent_;

  /// Stores a handle to the state.
  mcast_sub_state_ptr<T> state_;
};

// Base type for *hot* operators that multicast data to subscribed observers.
template <class T>
class mcast : public hot<T>, public ucast_sub_state_listener<T> {
public:
  // -- member types -----------------------------------------------------------

  using super = hot<T>;

  using state_type = mcast_sub_state<T>;

  using state_ptr_type = mcast_sub_state_ptr<T>;

  using observer_type = observer<T>;

  // -- constructors, destructors, and assignment operators --------------------

  explicit mcast(coordinator* parent) : super(parent) {
    // nop
  }

  ~mcast() override {
    close();
  }

  // -- broadcasting -----------------------------------------------------------

  /// Pushes @p item to all subscribers.
  /// @returns `true` if all observers consumed the item immediately without
  ///          buffering it, `false` otherwise.
  bool push_all(const T& item) {
    // Note: we can't use all_of here because we need to call push on all
    //      observers and the algorithm would short-circuit.
    return std::accumulate(states_.begin(), states_.end(), true,
                           [&item](bool res, const state_ptr_type& ptr) {
                             return res & ptr->push(item);
                           });
  }

  /// Closes the operator, eventually emitting on_complete on all observers.
  void close() {
    if (!closed_) {
      closed_ = true;
      for (auto& state : states_) {
        state->listener = nullptr;
        state->close();
      }
      states_.clear();
    }
  }

  /// Closes the operator, eventually emitting on_error on all observers.
  void abort(const error& reason) {
    if (!closed_) {
      closed_ = true;
      for (auto& state : states_) {
        state->listener = nullptr;
        state->abort(reason);
      }
      states_.clear();
      err_ = reason;
    }
  }

  // -- properties -------------------------------------------------------------

  size_t max_demand() const noexcept {
    return std::accumulate(states_.begin(), states_.end(), size_t{0},
                           [](size_t res, const state_ptr_type& state) {
                             return std::max(res, state->demand);
                           });
  }

  size_t min_demand() const noexcept {
    if (states_.empty()) {
      return 0;
    }
    return std::accumulate(states_.begin() + 1, states_.end(),
                           states_.front()->demand,
                           [](size_t res, const state_ptr_type& state) {
                             return std::min(res, state->demand);
                           });
  }

  size_t max_buffered() const noexcept {
    return std::accumulate(states_.begin(), states_.end(), size_t{0},
                           [](size_t res, const state_ptr_type& state) {
                             return std::max(res, state->buf.size());
                           });
  }

  size_t min_buffered() const noexcept {
    if (states_.empty()) {
      return 0;
    }
    return std::accumulate(states_.begin() + 1, states_.end(),
                           states_.front()->buf.size(),
                           [](size_t res, const state_ptr_type& state) {
                             return std::min(res, state->buf.size());
                           });
  }

  /// Queries whether there is at least one observer subscribed to the operator.
  bool has_observers() const noexcept {
    return !states_.empty();
  }

  /// Queries the current number of subscribed observers.
  size_t observer_count() const noexcept {
    return states_.size();
  }

  /// @private
  const auto& observers() const noexcept {
    return states_;
  }

  // -- state management -------------------------------------------------------

  /// Adds state for a new observer to the operator.
  state_ptr_type add_state(observer_type out) {
    auto state = super::parent_->add_child(std::in_place_type<state_type>,
                                           std::move(out));
    state->listener = this;
    states_.push_back(state);
    return state;
  }

  // -- implementation of observable -------------------------------------------

  /// Adds a new observer to the operator.
  disposable subscribe(observer_type out) override {
    if (!closed_) {
      auto ptr = super::parent_->add_child(std::in_place_type<mcast_sub<T>>,
                                           add_state(out));
      out.on_subscribe(subscription{ptr});
      return disposable{std::move(ptr)};
    }
    if (!err_) {
      return super::empty_subscription(out);
    }
    return super::fail_subscription(out, err_);
  }

  // -- implementation of ucast_sub_state_listener -----------------------------

  void on_disposed(state_type* ptr, bool from_external) final {
    super::parent_->delay_fn(
      [mc = strong_this(), sptr = state_ptr_type{ptr}, from_external] {
        if (auto i = std::find(mc->states_.begin(), mc->states_.end(), sptr);
            i != mc->states_.end()) {
          // We don't care about preserving the order of elements in the vector.
          // Hence, we can swap the element to the back and then pop it.
          auto last = mc->states_.end() - 1;
          if (i != last)
            std::swap(*i, *last);
          mc->states_.pop_back();
          mc->do_dispose(sptr, from_external);
        }
      });
  }

protected:
  bool closed_ = false;
  error err_;
  std::vector<state_ptr_type> states_;

private:
  intrusive_ptr<mcast> strong_this() {
    return {this};
  }

  /// Called whenever a state is disposed.
  virtual void do_dispose(const state_ptr_type&, bool) {
    // nop
  }
};

} // namespace caf::flow::op
