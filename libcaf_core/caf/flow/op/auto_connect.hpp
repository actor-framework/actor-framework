// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/atomic_ref_counted.hpp"
#include "caf/disposable.hpp"
#include "caf/error.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/op/base.hpp"
#include "caf/flow/op/connectable.hpp"
#include "caf/flow/subscription.hpp"

#include <memory>
#include <variant>

namespace caf::flow::op {

/// The state of the auto-connect operator. Shared between the operator and the
/// subscriptions objects that it creates.
template <class T>
class auto_connect_state {
public:
  using connectable_ptr = intrusive_ptr<connectable<T>>;

  auto_connect_state(size_t threshold, connectable_ptr source)
    : threshold(threshold), maybe_source(std::move(source)) {
    // nop
  }

  template <class OnConnect>
  bool connect(OnConnect&& on_connect) {
    auto src = std::get<connectable_ptr>(maybe_source);
    conn = src->connect();
    if (!conn) {
      if (can_connect()) {
        // If we reach here, it means that the source returned a disposed or
        // invalid subscription but did not call `on_error`.
        maybe_source = make_error(sec::invalid_observable);
      }
      return false;
    }
    if (!can_connect()) {
      conn.dispose();
      return false;
    }
    on_connect(src);
    threshold = 1; // threshold only applies to the first connect
    return true;
  }

  const connectable_ptr& source() const {
    return std::get<connectable_ptr>(maybe_source);
  }

  bool can_connect() const noexcept {
    return std::holds_alternative<connectable_ptr>(maybe_source);
  }

  bool connected() const noexcept {
    return conn.valid();
  }

  template <class OnConnect>
  bool inc_subscriber_count(OnConnect&& on_connect) {
    if (!can_connect()) {
      return false;
    }
    if (++subscriber_count == threshold && !conn) {
      return connect(std::forward<OnConnect>(on_connect));
    }
    return true;
  }

  void dec_subscriber_count() {
    if (--subscriber_count == 0 && auto_disconnect) {
      conn.dispose();
    }
  }

  void on_complete() {
    --subscriber_count;
    if (can_connect()) {
      maybe_source = error{};
      conn.dispose();
    }
  }

  void on_error(const error& what) {
    if (what == sec::disposed) {
      dec_subscriber_count();
      return;
    }
    if (can_connect()) {
      maybe_source = what;
      conn.dispose();
    }
  }

  /// Stores the number of current subscribers.
  size_t subscriber_count = 0;

  /// The number of subscribers required to connect to the source.
  size_t threshold;

  /// The source observable to connect to.
  std::variant<connectable_ptr, error> maybe_source;

  /// The connection to the source observable.
  disposable conn;

  /// Whether to disconnect from the source observable when the last subscriber
  /// cancels its subscription.
  bool auto_disconnect = false;
};

/// A smart pointer to the state of the auto-connect operator.
template <class T>
using auto_connect_state_ptr = std::shared_ptr<auto_connect_state<T>>;

/// Acts as intermediate between the source observable and the observer. Injects
/// additional bookkeeping for the auto-connect operator.
template <class T>
class auto_connect_subscription : public detail::atomic_ref_counted,
                                  public subscription_impl,
                                  public observer_impl<T>,
                                  public disposable_impl {
public:
  auto_connect_subscription(coordinator* parent,
                            auto_connect_state_ptr<T> state, observer<T> out)
    : parent_(parent), state_(std::move(state)), out_(std::move(out)) {
    // nop
  }

  coordinator* parent() const noexcept override {
    return parent_;
  }

  void request(size_t n) override {
    if (sub_) {
      sub_.request(n);
      return;
    }
    // If we didn't subscribe to the source yet, we store the demand for later.
    initial_demand_ += n;
  }

  void cancel() override {
    if (state_) {
      state_->dec_subscriber_count();
      state_ = nullptr;
    }
    // Note: calling cancel on the subscription may cause the subscription to
    // call `on_error(sec::disposed)`. Hence, we need to release the reference
    // to the observer first.
    out_.release_later();
    sub_.cancel();
  }

  void on_subscribe(subscription sub) override {
    if (!state_) {
      sub.cancel();
      return;
    }
    sub_ = std::move(sub);
    // If `request` was called before we subscribed to the source, forward the
    // pending demand.
    if (initial_demand_ > 0) {
      sub_.request(initial_demand_);
    }
  }

  void on_next(const T& item) override {
    out_.on_next(item);
  }

  void on_complete() override {
    if (out_) {
      out_.on_complete();
    }
    if (state_) {
      state_->on_complete();
      state_ = nullptr;
    }
  }

  void on_error(const error& what) override {
    if (out_) {
      out_.on_error(what);
    }
    if (state_) {
      if (what == sec::disposed) {
        // The subscription got disposed, which means this wasn't an actual
        // error from the source.
        state_->dec_subscriber_count();
      } else {
        state_->on_error(what);
      }
      state_ = nullptr;
    }
  }

  void dispose() override {
    if (disposed()) {
      return;
    }
    auto state = std::move(state_);
    auto out = std::move(out_);
    auto sub = std::move(sub_);
    parent_->delay_fn([state, out, sub]() mutable {
      if (state) {
        state->dec_subscriber_count();
      }
      if (out) {
        out.on_error(make_error(sec::disposed));
      }
      if (sub) {
        sub.cancel();
      }
    });
  }

  bool disposed() const noexcept override {
    return !state_;
  }

  // -- reference counting -----------------------------------------------------

  void ref_coordinated() const noexcept override {
    ref();
  }

  void deref_coordinated() const noexcept override {
    deref();
  }

  void ref_disposable() const noexcept override {
    ref();
  }

  void deref_disposable() const noexcept override {
    deref();
  }

  friend void
  intrusive_ptr_add_ref(const auto_connect_subscription* ptr) noexcept {
    ptr->ref();
  }

  friend void
  intrusive_ptr_release(const auto_connect_subscription* ptr) noexcept {
    ptr->deref();
  }

private:
  /// The parent coordinator.
  coordinator* parent_;

  /// Our state object. Shared with the operator and other subscriptions.
  auto_connect_state_ptr<T> state_;

  /// The observer to forward the items to. Shared with the operator.
  observer<T> out_;

  /// The subscription to the source observable.
  subscription sub_;

  /// The pending demand that was requested before we subscribed to the source.
  size_t initial_demand_ = 0;
};

/// Turns a connectable into an observable that automatically connects to the
/// source when reaching the subscriber threshold.
template <class T>
class auto_connect : public base<T>, public detail::atomic_ref_counted {
public:
  using super = base<T>;

  auto_connect(coordinator* parent, size_t threshold,
               intrusive_ptr<connectable<T>> source)
    : parent_(parent),
      state_(std::make_shared<auto_connect_state<T>>(threshold, source)) {
  }

  ~auto_connect() {
    if (!pending_subscriptions_.empty()) {
      auto err = make_error(sec::disposed);
      for (auto& ptr : pending_subscriptions_) {
        ptr->on_error(err);
      }
    }
  }

  coordinator* parent() const noexcept override {
    return parent_;
  }

  disposable subscribe(observer<T> what) override {
    // Utility function to fail or complete the subscription immediately in case
    // the source is no longer connectable.
    auto short_circuit = [this, &what] {
      if (auto& err = std::get<error>(state_->maybe_source); err.valid()) {
        return super::fail_subscription(what, err);
      }
      return super::empty_subscription(what);
    };
    // Short-circuit if the source has already completed or failed.
    if (!state_->can_connect()) {
      return short_circuit();
    }
    // Increment the subscriber count, auto-connecting if necessary.
    auto ok = state_->inc_subscriber_count([this](const auto& source) {
      for (auto& ptr : pending_subscriptions_) {
        source->subscribe(ptr->as_observer());
      }
      pending_subscriptions_.clear();
    });
    // If inc_subscriber_count returns false, it means that we failed to connect
    // to the source and it is no longer connectable.
    if (!ok) {
      CAF_ASSERT(!state_->can_connect());
      return short_circuit();
    }
    // Create a new subscription that links to the state.
    auto ptr = parent_->add_child(std::in_place_type<subscription_type>, state_,
                                  what);
    what.on_subscribe(subscription{ptr});
    // If the source cancels immediately, we can discard the subscription again.
    if (ptr->disposed()) {
      return {};
    }
    // If we are already connected, we can immediately subscribe to the source.
    // Otherwise, we defer subscribing to the source until we connect.
    if (state_->connected()) {
      state_->source()->subscribe(ptr->as_observer());
    } else {
      pending_subscriptions_.emplace_back(ptr);
    }
    return ptr->as_disposable();
  }

  void ref_coordinated() const noexcept override {
    this->ref();
  }

  void deref_coordinated() const noexcept override {
    this->deref();
  }

  size_t pending_subscriptions_count() const {
    return pending_subscriptions_.size();
  }

  bool connected() const {
    return state_->connected();
  }

  auto& state() {
    return *state_;
  }

protected:
  using subscription_type = auto_connect_subscription<T>;

  /// The parent coordinator.
  coordinator* parent_;

  /// The state of the auto-connect operator. Shared with the subscriptions.
  auto_connect_state_ptr<T> state_;

  /// The pending subscriptions that need to be added to the source when the
  /// auto-connect event triggers.
  std::vector<intrusive_ptr<subscription_type>> pending_subscriptions_;
};

} // namespace caf::flow::op
