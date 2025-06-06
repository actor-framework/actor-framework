// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/flow/observable_decl.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/op/cold.hpp"
#include "caf/flow/step/all.hpp"
#include "caf/flow/subscription.hpp"

namespace caf::flow::op {

struct debounce_input_t {};

/// The subscription for the `debounce` operator.
template <class T>
class debounce_sub : public subscription::impl_base {
public:
  // -- member types -----------------------------------------------------------

  using input_type = T;

  using output_type = T;

  // -- constructors, destructors, and assignment operators --------------------

  debounce_sub(coordinator* parent, observer<output_type> out, timespan period)
    : parent_(parent), out_(std::move(out)), period_(period) {
    fire_action_ = make_action([this] { fire(); });
  }

  ~debounce_sub() {
    fire_action_.dispose();
  }

  // -- properties -------------------------------------------------------------

  coordinator* parent() const noexcept override {
    return parent_;
  }

  bool running() const noexcept {
    return out_.valid();
  }

  const error& err() const noexcept {
    return err_;
  }

  bool pending() const noexcept {
    return pending_.valid();
  }

  // -- callbacks for the parent -----------------------------------------------

  void init(observable<input_type> vals) {
    using val_fwd_t = forwarder<input_type, debounce_sub, debounce_input_t>;
    auto fwd = parent_->add_child(std::in_place_type<val_fwd_t>, this,
                                  debounce_input_t{});
    vals.subscribe(fwd->as_observer());
  }

  // -- callbacks for the forwarders -------------------------------------------

  void fwd_on_subscribe(debounce_input_t, subscription sub) {
    if (sub_ || !out_) {
      sub.cancel();
      return;
    }
    sub_ = std::move(sub);
    sub_.request(1);
  }

  void fwd_on_complete(debounce_input_t) {
    // If we don't have a value to emit, we can just complete immediately.
    if (!buf_) {
      shutdown();
      return;
    }
    // Deliver the last value immediately if we have the demand necessary.
    if (demand_ > 0) {
      --demand_;
      out_.on_next(*buf_);
      buf_.reset();
      shutdown();
      return;
    }
    // Otherwise, we will have to wait for the observer to request more items.
    completed_ = true;
  }

  void fwd_on_error(debounce_input_t, const error& what) {
    // We will call `shutdown()` in `fwd_on_complete()`, which will respect
    // `err_`. Hence, we can dispatch to `fwd_on_complete()` here.
    err_ = what;
    fwd_on_complete(debounce_input_t{});
  }

  void fwd_on_next(debounce_input_t, const input_type& item) {
    if (!running())
      return;
    buf_ = item;
    sub_.request(1);
    due_ = parent_->steady_time() + period_;
    if (!pending_ && demand_ > 0) {
      pending_ = parent_->delay_until(due_, fire_action_);
    }
  }

  // -- implementation of subscription -----------------------------------------

  bool disposed() const noexcept override {
    return !out_;
  }

  void request(size_t n) override {
    CAF_ASSERT(out_.valid());
    demand_ += n;
    if (demand_ == n && !pending_) {
      fire();
    }
  }

private:
  void fire() {
    pending_ = disposable{};
    if (demand_ == 0 || !out_)
      return;
    if (buf_) {
      if (due_ <= parent_->steady_time()) {
        --demand_;
        out_.on_next(*buf_);
        buf_.reset();
        if (completed_) {
          shutdown();
        }
      } else {
        pending_ = parent_->delay_until(due_, fire_action_);
      }
    }
  }

  void do_dispose(bool from_external) override {
    if (!out_)
      return;
    pending_.dispose();
    fire_action_.dispose();
    sub_.cancel();
    if (from_external)
      out_.on_error(make_error(sec::disposed));
    else
      out_.release_later();
  }

  void shutdown() {
    pending_.dispose();
    fire_action_.dispose();
    sub_.cancel();
    if (!err_)
      out_.on_complete();
    else
      out_.on_error(err_);
  }

  /// Stores the context (coordinator) that runs this flow.
  coordinator* parent_;

  /// Stores the element until we can emit it.
  std::optional<input_type> buf_;

  /// Stores a handle to the subscribed observer.
  observer<output_type> out_;

  /// Our subscription for the values. We request `max_buf_size_` items and
  /// whenever we emit a batch, we request whatever amount we have shipped. That
  /// way, we should always have enough demand at the source to fill up a batch.
  subscription sub_;

  /// Demand signaled by the observer.
  size_t demand_ = 0;

  /// Caches the abort reason.
  error err_;

  /// Sequence of control messages to select previous inputs.
  timespan period_;

  /// Handle to the pending timeout.
  disposable pending_;

  /// Stores whether `on_complete()` has been called. We need to keep track of
  /// this because the `on_complete()` callback may be called before we can
  /// deliver the last value.
  bool completed_ = false;

  /// Stores the time point when the next element should be emitted.
  coordinator::steady_time_point due_;

  /// Action to fire the current element. Repeatedly called whenever the
  /// current element is updated.
  action fire_action_;
};

template <class T>
class debounce : public cold<T> {
public:
  // -- member types -----------------------------------------------------------

  using input_type = T;

  using output_type = T;

  using super = cold<output_type>;

  using input = observable<input_type>;

  // -- constructors, destructors, and assignment operators --------------------

  debounce(coordinator* parent, input in, timespan period)
    : super(parent), in_(std::move(in)), period_(std::move(period)) {
    // nop
  }

  // -- implementation of observable<T> -----------------------------------

  disposable subscribe(observer<output_type> out) override {
    auto ptr = super::parent_->add_child(std::in_place_type<debounce_sub<T>>,
                                         out, period_);
    ptr->init(in_);
    if (!ptr->running()) {
      return super::fail_subscription(
        out, ptr->err().or_else(sec::runtime_error,
                                "failed to initialize debounce subscription"));
    }

    out.on_subscribe(subscription{ptr});
    return ptr->as_disposable();
  }

private:
  /// Sequence of input values.
  input in_;

  /// The debounce period.
  timespan period_;
};

} // namespace caf::flow::op
