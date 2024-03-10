// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/flow/observable_decl.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/op/cold.hpp"
#include "caf/flow/op/state.hpp"
#include "caf/flow/step/all.hpp"
#include "caf/flow/subscription.hpp"
#include "caf/unit.hpp"

#include <vector>

namespace caf::flow::op {

struct debounce_input_t {};

/// The subsciption for the `debounce` operator.
template <class T>
class debounce_sub : public subscription::impl_base {
public:
  // -- member types -----------------------------------------------------------

  using input_type = T;

  using output_type = T;

  // -- constructors, destructors, and assignment operators --------------------

  debounce_sub(coordinator* parent, observer<output_type> out, timespan period)
    : parent_(parent), out_(std::move(out)), period_(period) {
    // nop
  }

  // -- properties -------------------------------------------------------------

  coordinator* parent() const noexcept override {
    return parent_;
  }

  bool running() const noexcept {
    return state_ == state::running;
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
    if (!running() || value_sub_ || !out_) {
      sub.cancel();
      return;
    }
    value_sub_ = std::move(sub);
    value_sub_.request(defaults::flow::buffer_size);
  }

  void fwd_on_complete(debounce_input_t) {
    if (pending()) {
      fire();
    }
    shutdown();
  }

  void fwd_on_error(debounce_input_t, const error& what) {
    if (pending()) {
      fire();
    }
    err_ = what;
    shutdown();
  }

  void fwd_on_next(debounce_input_t, const input_type& item) {
    if (!running())
      return;
    buf_ = item;
    value_sub_.request(1);
    if (pending())
      pending_.dispose();
    pending_ = parent_->delay_for_fn(
      period_, [sptr = intrusive_ptr<debounce_sub>{this}] { //
        sptr->fire();
      });
  }

  // -- implementation of subscription -----------------------------------------

  bool disposed() const noexcept override {
    return !out_;
  }

  void request(size_t n) override {
    CAF_ASSERT(out_.valid());
    demand_ += n;
  }

private:
  void fire() {
    if (demand_ == 0)
      return;
    --demand_;
    auto debounced = buf_.has_value();
    if (debounced) {
      out_.on_next(*buf_);
      buf_.reset();
      pending_.dispose();
    }
  }

  void do_dispose(bool from_external) override {
    if (!out_)
      return;
    state_ = state::disposed;
    value_sub_.cancel();
    if (from_external)
      out_.on_error(make_error(sec::disposed));
    else
      out_.release_later();
  }

  void shutdown() {
    value_sub_.cancel();
    if (!err_)
      out_.on_complete();
    else
      out_.on_error(err_);
    state_ = err_ ? state::aborted : state::disposed;
  }

  /// Stores the context (coordinator) that runs this flow.
  coordinator* parent_;

  /// Stores the elements until we can emit them.
  std::optional<input_type> buf_;

  /// Stores a handle to the subscribed observer.
  observer<output_type> out_;

  /// Our subscription for the values. We request `max_buf_size_` items and
  /// whenever we emit a batch, we request whatever amount we have shipped. That
  /// way, we should always have enough demand at the source to fill up a batch.
  subscription value_sub_;

  /// Demand signaled by the observer.
  size_t demand_ = 0;

  /// Our current state.
  /// - running: alive and ready to emit batches.
  /// - completed: on_complete was called but some data is still debounced.
  /// - aborted: on_error was called but some data is still debounced.
  /// - disposed: inactive.
  state state_ = state::running;

  /// Caches the abort reason.
  error err_;

  /// Sequence of control messages to select previous inputs.
  timespan period_;

  /// Handle to the pending timeout.
  disposable pending_;
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
