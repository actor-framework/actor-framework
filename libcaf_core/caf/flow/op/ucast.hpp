// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/flow/coordinator.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/op/empty.hpp"
#include "caf/flow/op/hot.hpp"
#include "caf/flow/subscription.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/make_counted.hpp"

#include <deque>
#include <memory>

namespace caf::flow::op {

/// State shared between one multicast operator and one subscribed observer.
template <class T>
class ucast_sub_state : public detail::plain_ref_counted {
public:
  friend void intrusive_ptr_add_ref(const ucast_sub_state* ptr) noexcept {
    ptr->ref();
  }

  friend void intrusive_ptr_release(const ucast_sub_state* ptr) noexcept {
    ptr->deref();
  }

  explicit ucast_sub_state(coordinator* ptr) : ctx(ptr) {
    // nop
  }

  ucast_sub_state(coordinator* ctx, observer<T> out)
    : ctx(ctx), out(std::move(out)) {
    // nop
  }

  coordinator* ctx;
  std::deque<T> buf;
  size_t demand = 0;
  observer<T> out;
  bool disposed = false;
  bool closed = false;
  bool running = false;
  error err;

  action when_disposed;
  action when_consumed_some;
  action when_demand_changed;

  void push(const T& item) {
    if (disposed) {
      // nop
    } else if (demand > 0 && !running) {
      CAF_ASSERT(out);
      CAF_ASSERT(buf.empty());
      --demand;
      out.on_next(item);
      if (when_consumed_some)
        ctx->delay(when_consumed_some);
    } else {
      buf.push_back(item);
    }
  }

  void close() {
    if (!disposed) {
      closed = true;
      if (!running && buf.empty()) {
        disposed = true;
        if (out) {
          out.on_complete();
          out = nullptr;
        }
        when_disposed = nullptr;
        when_consumed_some = nullptr;
        when_demand_changed = nullptr;
      }
    }
  }

  void abort(const error& reason) {
    if (!disposed && !err) {
      closed = true;
      err = reason;
      if (!running && buf.empty()) {
        disposed = true;
        if (out) {
          out.on_error(reason);
          out = nullptr;
        }
        when_disposed = nullptr;
        when_consumed_some = nullptr;
        when_demand_changed = nullptr;
      }
    }
  }

  void do_dispose() {
    if (out) {
      out.on_complete();
      out = nullptr;
    }
    if (when_disposed) {
      ctx->delay(std::move(when_disposed));
    }
    if (when_consumed_some) {
      when_consumed_some.dispose();
      when_consumed_some = nullptr;
    }
    when_demand_changed = nullptr;
    buf.clear();
    demand = 0;
    disposed = true;
  }

  void do_run() {
    auto guard = detail::make_scope_guard([this] { running = false; });
    if (!disposed) {
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
        if (err)
          out.on_error(err);
        else
          out.on_complete();
        out = nullptr;
        do_dispose();
      } else if (got_some && when_consumed_some) {
        ctx->delay(when_consumed_some);
      }
    }
  }
};

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
    return !state_;
  }

  void dispose() override {
    if (state_) {
      ctx_->delay_fn([state = std::move(state_)]() { state->do_dispose(); });
    }
  }

  void request(size_t n) override {
    if (!state_)
      return;
    state_->demand += n;
    if (state_->when_demand_changed)
      state_->when_demand_changed.run();
    if (!state_->running) {
      state_->running = true;
      ctx_->delay_fn([state = state_] { state->do_run(); });
    }
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
    state_->push(item);
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
        out.on_error(state_->err);
        return disposable{};
      }
      return make_counted<op::empty<T>>(super::ctx_)->subscribe(out);
    }
    if (state_->out) {
      auto err = make_error(sec::too_many_observers,
                            "may only subscribe once to an unicast operator");
      out.on_error(err);
      return disposable{};
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
