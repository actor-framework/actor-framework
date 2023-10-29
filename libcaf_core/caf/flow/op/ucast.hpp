// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/flow/coordinator.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/op/hot.hpp"
#include "caf/flow/op/pullable.hpp"
#include "caf/flow/subscription.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/make_counted.hpp"

#include <deque>
#include <memory>

namespace caf::flow::op {

/// Shared state between an operator that emits values and the subscribed
/// observer.
template <class T>
class ucast_sub_state : public detail::plain_ref_counted, public pullable {
public:
  // -- friends ----------------------------------------------------------------

  friend void intrusive_ptr_add_ref(const ucast_sub_state* ptr) noexcept {
    ptr->ref();
  }

  friend void intrusive_ptr_release(const ucast_sub_state* ptr) noexcept {
    ptr->deref();
  }

  // -- member types -----------------------------------------------------------

  /// Interface for listeners that want to be notified when a `ucast_sub_state`
  /// is disposed, has consumed some items, or when its demand hast changed.
  class abstract_listener {
  public:
    virtual ~abstract_listener() {
      // nop
    }

    /// Called when the `ucast_sub_state` is disposed.
    virtual void on_disposed(ucast_sub_state*) = 0;

    /// Called when the `ucast_sub_state` receives new demand.
    virtual void on_demand_changed(ucast_sub_state*) {
      // nop
    }

    /// Called when the `ucast_sub_state` has consumed some items.
    /// @param state The `ucast_sub_state` that consumed items.
    /// @param old_buffer_size The number of items in the buffer before
    ///                        consuming items.
    /// @param new_buffer_size The number of items in the buffer after
    ///                        consuming items.
    virtual void on_consumed_some([[maybe_unused]] ucast_sub_state* state,
                                  [[maybe_unused]] size_t old_buffer_size,
                                  [[maybe_unused]] size_t new_buffer_size) {
      // nop
    }
  };

  // -- constructors, destructors, and assignment operators --------------------

  explicit ucast_sub_state(coordinator* ptr) : ctx(ptr) {
    // nop
  }

  ucast_sub_state(coordinator* ptr, observer<T> obs)
    : ctx(ptr), out(std::move(obs)) {
    // nop
  }

  /// The coordinator for scheduling delayed function calls.
  coordinator* ctx;

  /// The buffer for storing items until the observer requests them.
  std::deque<T> buf;

  /// The number items that the observer has requested but not yet received.
  size_t demand = 0;

  /// The observer to send items to.
  observer<T> out;

  /// Keeps track of whether this object has been disposed.
  bool disposed = false;

  /// Keeps track of whether this object has been closed.
  bool closed = false;

  /// The error to pass to the observer after the last `on_next` call. If this
  /// error is default-constructed, then the observer receives `on_complete`.
  /// Otherwise, the observer receives `on_error`.
  error err;

  /// The listener for state changes. We hold a non-owning pointer to the
  /// listener, because the listener owns the state.
  abstract_listener* listener = nullptr;

  /// Returns `true` if `item` was consumed, `false` when it was buffered.
  [[nodiscard]] bool push(const T& item) {
    if (disposed)
      return true;
    if (demand > 0 && !this->is_pulling()) {
      CAF_ASSERT(out);
      CAF_ASSERT(buf.empty());
      --demand;
      out.on_next(item);
      return true;
    } else {
      buf.push_back(item);
      return false;
    }
  }

  void close() {
    if (!disposed) {
      closed = true;
      if (!this->is_pulling() && buf.empty()) {
        disposed = true;
        listener = nullptr;
        if (out) {
          auto tmp = std::move(out);
          tmp.on_complete();
        }
      }
    }
  }

  void request(size_t n) {
    // If we have data buffered, we need to schedule a call to do_run in order
    // to have a safe context for calling on_next. Otherwise, we can simply
    // increment our demand counter. We can also increment the demand counter if
    // we are already running, because then we know that we are in a safe
    // context.
    if (buf.empty()) {
      demand += n;
      if (listener)
        listener->on_demand_changed(this);
    } else {
      this->pull(ctx, n);
    }
  }

  void abort(const error& reason) {
    if (!disposed && !err) {
      closed = true;
      err = reason;
      if (!this->is_pulling() && buf.empty()) {
        disposed = true;
        listener = nullptr;
        if (out) {
          auto tmp = std::move(out);
          tmp.on_error(reason);
        }
      }
    }
  }

  void dispose() {
    buf.clear();
    demand = 0;
    disposed = true;
    if (listener) {
      auto* lptr = listener;
      listener = nullptr;
      lptr->on_disposed(this);
    }
    if (out) {
      auto tmp = std::move(out);
      tmp.on_complete();
    }
  }

private:
  void do_pull(size_t n) override {
    if (!disposed) {
      demand += n;
      if (listener)
        listener->on_demand_changed(this);
      auto old_buf_size = buf.size();
      auto got_some = demand > 0 && !buf.empty();
      for (bool run = got_some; run; run = demand > 0 && !buf.empty()) {
        out.on_next(buf.front());
        // Note: on_next may call dispose().
        if (disposed)
          return;
        buf.pop_front();
        --demand;
      }
      if (buf.empty() && closed) {
        auto tmp = std::move(out);
        if (err)
          tmp.on_error(err);
        else
          tmp.on_complete();
        dispose();
      } else if (got_some && listener) {
        listener->on_consumed_some(this, old_buf_size, buf.size());
      }
    }
  }

  void do_ref() override {
    this->ref();
  }

  void do_deref() override {
    this->deref();
  }
};

template <class T>
using ucast_sub_state_listener = typename ucast_sub_state<T>::abstract_listener;

template <class T>
using ucast_sub_state_ptr = intrusive_ptr<ucast_sub_state<T>>;

template <class T>
class ucast_sub : public subscription::impl_base {
public:
  // -- constructors, destructors, and assignment operators --------------------

  ucast_sub(coordinator* ctx, ucast_sub_state_ptr<T> state)
    : ctx_(ctx), state_(std::move(state)) {
    // nop
  }

  // -- implementation of subscription -----------------------------------------

  bool disposed() const noexcept override {
    return !state_ || state_->disposed;
  }

  void dispose() override {
    if (state_) {
      auto state = std::move(state_);
      state->dispose();
    }
  }

  void request(size_t n) override {
    if (state_)
      state_->request(n);
  }

private:
  /// Stores the context (coordinator) that runs this flow.
  coordinator* ctx_;

  /// Stores a handle to the state.
  ucast_sub_state_ptr<T> state_;
};

// Base type for *hot* operators that "unicast" data to a subscribed observer.
template <class T>
class ucast : public hot<T> {
public:
  // -- member types -----------------------------------------------------------

  using super = hot<T>;

  using state_type = ucast_sub_state<T>;

  using state_ptr_type = ucast_sub_state_ptr<T>;

  using observer_type = observer<T>;

  // -- constructors, destructors, and assignment operators --------------------

  explicit ucast(coordinator* ctx) : super(ctx) {
    state_ = make_counted<ucast_sub_state<T>>(ctx);
  }

  /// Pushes @p item to the subscriber or buffers them until subscribed.
  void push(const T& item) {
    std::ignore = state_->push(item);
  }

  /// Closes the operator, eventually emitting on_complete on all observers.
  void close() {
    state_->close();
  }

  /// Closes the operator, eventually emitting on_error on all observers.
  void abort(const error& reason) {
    state_->abort(reason);
  }

  size_t demand() const noexcept {
    return state_->demand;
  }

  size_t buffered() const noexcept {
    return state_->buf.size();
  }

  /// Queries the current number of subscribed observers.
  size_t has_observer() const noexcept {
    return state_.out.valid();
  }

  bool disposed() const noexcept {
    return state_->disposed;
  }

  state_type& state() {
    return *state_;
  }

  state_ptr_type state_ptr() {
    return state_;
  }

  disposable subscribe(observer<T> out) override {
    if (state_->closed) {
      if (state_->err) {
        return fail_subscription(out, state_->err);
      }
      return empty_subscription(out);
    }
    if (state_->out) {
      return fail_subscription(
        out, make_error(sec::too_many_observers,
                        "may only subscribe once to an unicast operator"));
    }
    state_->out = out;
    auto sub_ptr = make_counted<ucast_sub<T>>(super::ctx(), state_);
    out.on_subscribe(subscription{sub_ptr});
    return disposable{sub_ptr};
  }

private:
  state_ptr_type state_;
};

/// @relates ucast
template <class T>
using ucast_ptr = intrusive_ptr<ucast<T>>;

} // namespace caf::flow::op
