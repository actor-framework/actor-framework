// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/cow_vector.hpp"
#include "caf/detail/assert.hpp"
#include "caf/flow/observable_decl.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/op/cold.hpp"
#include "caf/flow/op/state.hpp"
#include "caf/flow/step/all.hpp"
#include "caf/flow/subscription.hpp"
#include "caf/unit.hpp"

#include <vector>

namespace caf::flow::op {

struct sample_input_t {};

struct sample_emit_t {};

/// The subscription for the `sample` operator.
template <class T>
class sample_sub : public subscription::impl_base {
public:
  // -- member types -----------------------------------------------------------

  using input_type = T;

  using output_type = T;

  using select_token_type = int64_t;

  // -- constructors, destructors, and assignment operators --------------------

  sample_sub(coordinator* parent, observer<output_type> out)
    : parent_(parent), out_(std::move(out)) {
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
    return buf_.has_value();
  }

  // -- callbacks for the parent -----------------------------------------------

  void init(observable<input_type> vals, observable<select_token_type> ctrl) {
    using val_fwd_t = forwarder<input_type, sample_sub, sample_input_t>;
    using ctrl_fwd_t = forwarder<select_token_type, sample_sub, sample_emit_t>;
    auto fwd = parent_->add_child(std::in_place_type<val_fwd_t>, this,
                                  sample_input_t{});
    vals.subscribe(fwd->as_observer());
    // Note: the previous subscribe might call on_error, in which case we don't
    // need to try to subscribe to the control observable.
    if (running())
      ctrl.subscribe(parent_->add_child_hdl(std::in_place_type<ctrl_fwd_t>,
                                            this, sample_emit_t{}));
  }

  // -- callbacks for the forwarders -------------------------------------------

  void fwd_on_subscribe(sample_input_t, subscription sub) {
    if (!running() || value_sub_ || !out_) {
      sub.cancel();
      return;
    }
    value_sub_ = std::move(sub);
    value_sub_.request(defaults::flow::buffer_size);
  }

  void fwd_on_complete(sample_input_t) {
    value_sub_.release_later();
  }

  void fwd_on_error(sample_input_t, const error& what) {
    value_sub_.release_later();
    err_ = what;
  }

  void fwd_on_next(sample_input_t, const input_type& item) {
    if (running()) {
      buf_ = item;
      value_sub_.request(1);
    }
  }

  void fwd_on_subscribe(sample_emit_t, subscription sub) {
    if (!running() || control_sub_ || !out_) {
      sub.cancel();
      return;
    }
    control_sub_ = std::move(sub);
    control_sub_.request(1);
  }

  void fwd_on_complete(sample_emit_t) {
    control_sub_.release_later();
    if (state_ == state::running)
      err_ = make_error(sec::end_of_stream,
                        "sample: unexpected end of the control stream");
    shutdown();
  }

  void fwd_on_error(sample_emit_t, const error& what) {
    control_sub_.release_later();
    err_ = what;
    shutdown();
  }

  void fwd_on_next(sample_emit_t, select_token_type) {
    if (!value_sub_) {
      shutdown();
      return;
    }
    if (demand_ == 0)
      return;
    --demand_;
    auto sampled = buf_.has_value();
    if (sampled) {
      out_.on_next(*buf_);
      buf_.reset();
    }
    control_sub_.request(1);
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
  void do_dispose(bool from_external) override {
    if (!out_)
      return;
    state_ = state::disposed;
    value_sub_.cancel();
    control_sub_.cancel();
    if (from_external)
      out_.on_error(make_error(sec::disposed));
    else
      out_.release_later();
  }

  void shutdown() {
    value_sub_.cancel();
    control_sub_.cancel();
    if (!err_)
      out_.on_complete();
    else
      out_.on_error(err_);
    state_ = err_ ? state::aborted : state::disposed;
  }

  void on_request() {
    if (demand_ == 0 || !buf_.has_value())
      return;
    if (!err_)
      out_.on_complete();
    else
      out_.on_error(err_);
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

  /// Our subscription for the control tokens. We always request 1 item.
  subscription control_sub_;

  /// Demand signaled by the observer.
  size_t demand_ = 0;

  /// Our current state.
  /// - running: alive and ready to emit batches.
  /// - completed: on_complete was called but some data is still sampled.
  /// - aborted: on_error was called but some data is still sampled.
  /// - disposed: inactive.
  state state_ = state::running;

  /// Caches the abort reason.
  error err_;
};

template <class T>
class sample : public cold<T> {
public:
  // -- member types -----------------------------------------------------------

  using input_type = T;

  using output_type = T;

  using super = cold<output_type>;

  using input = observable<input_type>;

  using selector = observable<int64_t>;

  // -- constructors, destructors, and assignment operators --------------------

  sample(coordinator* parent, input in, selector select)
    : super(parent), in_(std::move(in)), select_(std::move(select)) {
    // nop
  }

  // -- implementation of observable<T> -----------------------------------

  disposable subscribe(observer<output_type> out) override {
    auto ptr = super::parent_->add_child(std::in_place_type<sample_sub<T>>,
                                         out);
    ptr->init(in_, select_);
    if (!ptr->running()) {
      return super::fail_subscription(
        out, ptr->err().or_else(sec::runtime_error,
                                "failed to initialize sample subscription"));
    }
    out.on_subscribe(subscription{ptr});
    return ptr->as_disposable();
  }

private:
  /// Sequence of input values.
  input in_;

  /// Sequence of control messages to select previous inputs.
  selector select_;
};

} // namespace caf::flow::op
