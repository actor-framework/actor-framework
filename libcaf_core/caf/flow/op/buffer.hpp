// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/cow_vector.hpp"
#include "caf/flow/observable_decl.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/op/cold.hpp"
#include "caf/flow/op/state.hpp"
#include "caf/flow/subscription.hpp"
#include "caf/unit.hpp"

#include <vector>

namespace caf::flow::op {

struct buffer_input_t {};

struct buffer_emit_t {};

template <class T>
struct buffer_default_trait {
  static constexpr bool skip_empty = false;
  using input_type = T;
  using output_type = cow_vector<T>;
  using select_token_type = unit_t;

  output_type operator()(const std::vector<input_type>& xs) {
    return output_type{xs};
  }
};

template <class T>
struct buffer_interval_trait {
  static constexpr bool skip_empty = false;
  using input_type = T;
  using output_type = cow_vector<T>;
  using select_token_type = int64_t;

  output_type operator()(const std::vector<input_type>& xs) {
    return output_type{xs};
  }
};

///
template <class Trait>
class buffer_sub : public subscription::impl_base {
public:
  // -- member types -----------------------------------------------------------

  using input_type = typename Trait::input_type;

  using output_type = typename Trait::output_type;

  using select_token_type = typename Trait::select_token_type;

  // -- constants --------------------------------------------------------------

  static constexpr size_t val_id = 0;

  static constexpr size_t ctrl_id = 1;

  // -- constructors, destructors, and assignment operators --------------------

  buffer_sub(coordinator* ctx, size_t max_buf_size, observer<output_type> out)
    : ctx_(ctx), max_buf_size_(max_buf_size), out_(std::move(out)) {
    // nop
  }

  // -- properties -------------------------------------------------------------

  bool running() const noexcept {
    return state_ == state::running;
  }

  const error& err() const noexcept {
    return err_;
  }

  size_t pending() const noexcept {
    return buf_.size();
  }

  bool can_emit() const noexcept {
    return buf_.size() == max_buf_size_ || has_shut_down(state_);
  }

  // -- callbacks for the parent -----------------------------------------------

  void init(observable<input_type> vals, observable<select_token_type> ctrl) {
    using val_fwd_t = forwarder<input_type, buffer_sub, buffer_input_t>;
    using ctrl_fwd_t = forwarder<select_token_type, buffer_sub, buffer_emit_t>;
    vals.subscribe(
      make_counted<val_fwd_t>(this, buffer_input_t{})->as_observer());
    // Note: the previous subscribe might call on_error, in which case we don't
    // need to try to subscribe to the control observable.
    if (running())
      ctrl.subscribe(
        make_counted<ctrl_fwd_t>(this, buffer_emit_t{})->as_observer());
  }

  // -- callbacks for the forwarders -------------------------------------------

  void fwd_on_subscribe(buffer_input_t, subscription sub) {
    if (!running() || value_sub_ || !out_) {
      sub.dispose();
      return;
    }
    value_sub_ = std::move(sub);
    value_sub_.request(max_buf_size_);
  }

  void fwd_on_complete(buffer_input_t) {
    value_sub_ = nullptr;
    shutdown();
  }

  void fwd_on_error(buffer_input_t, const error& what) {
    value_sub_ = nullptr;
    err_ = what;
    shutdown();
  }

  void fwd_on_next(buffer_input_t, const input_type& item) {
    if (running()) {
      buf_.push_back(item);
      if (buf_.size() == max_buf_size_)
        do_emit();
    }
  }

  void fwd_on_subscribe(buffer_emit_t, subscription sub) {
    if (!running() || control_sub_ || !out_) {
      sub.dispose();
      return;
    }
    control_sub_ = std::move(sub);
    control_sub_.request(1);
  }

  void fwd_on_complete(buffer_emit_t) {
    control_sub_ = nullptr;
    if (state_ == state::running)
      err_ = make_error(sec::end_of_stream,
                        "buffer: unexpected end of the control stream");
    shutdown();
  }

  void fwd_on_error(buffer_emit_t, const error& what) {
    control_sub_ = nullptr;
    err_ = what;
    shutdown();
  }

  void fwd_on_next(buffer_emit_t, select_token_type) {
    if constexpr (Trait::skip_empty) {
      if (!buf_.empty())
        do_emit();
    } else {
      do_emit();
    }
    control_sub_.request(1);
  }

  // -- implementation of subscription -----------------------------------------

  bool disposed() const noexcept override {
    return !out_;
  }

  void dispose() override {
    if (out_) {
      ctx_->delay_fn([strong_this = intrusive_ptr<buffer_sub>{this}] {
        strong_this->do_dispose();
      });
    }
  }

  void request(size_t n) override {
    CAF_ASSERT(out_.valid());
    demand_ += n;
    // If we can ship a batch, schedule an event to do so.
    if (demand_ == n && can_emit()) {
      ctx_->delay_fn([strong_this = intrusive_ptr<buffer_sub>{this}] {
        strong_this->on_request();
      });
    }
  }

private:
  void shutdown() {
    value_sub_.dispose();
    control_sub_.dispose();
    switch (state_) {
      case state::running:
        if (!buf_.empty()) {
          if (demand_ == 0) {
            state_ = err_ ? state::aborted : state::completed;
            return;
          }
          Trait f;
          out_.on_next(f(buf_));
          buf_.clear();
        }
        if (err_)
          out_.on_error(err_);
        else
          out_.on_complete();
        out_ = nullptr;
        state_ = state::disposed;
        break;
      default:
        break;
    }
  }

  void on_request() {
    if (demand_ == 0 || !can_emit())
      return;
    if (running()) {
      CAF_ASSERT(buf_.size() == max_buf_size_);
      do_emit();
      return;
    }
    if (!buf_.empty())
      do_emit();
    if (err_)
      out_.on_error(err_);
    else
      out_.on_complete();
    out_ = nullptr;
  }

  void do_emit() {
    if (demand_ == 0)
      return;
    Trait f;
    --demand_;
    auto buffered = buf_.size();
    out_.on_next(f(buf_));
    buf_.clear();
    if (value_sub_ && buffered > 0)
      value_sub_.request(buffered);
  }

  void do_dispose() {
    value_sub_.dispose();
    control_sub_.dispose();
    if (out_) {
      out_.on_complete();
      out_ = nullptr;
    }
    state_ = state::disposed;
  }

  /// Stores the context (coordinator) that runs this flow.
  coordinator* ctx_;

  /// Stores the maximum buffer size before forcing a batch.
  size_t max_buf_size_;

  /// Stores the elements until we can emit them.
  std::vector<input_type> buf_;

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
  /// - completed: on_complete was called but some data is still buffered.
  /// - aborted: on_error was called but some data is still buffered.
  /// - disposed: inactive.
  state state_ = state::running;

  /// Caches the abort reason.
  error err_;
};

template <class Trait>
class buffer : public cold<typename Trait::output_type> {
public:
  // -- member types -----------------------------------------------------------

  using input_type = typename Trait::input_type;

  using output_type = typename Trait::output_type;

  using super = cold<output_type>;

  using input = observable<input_type>;

  using selector = observable<typename Trait::select_token_type>;

  // -- constructors, destructors, and assignment operators --------------------

  buffer(coordinator* ctx, size_t max_items, input in, selector select)
    : super(ctx),
      max_items_(max_items),
      in_(std::move(in)),
      select_(std::move(select)) {
    // nop
  }

  // -- implementation of observable<T> -----------------------------------

  disposable subscribe(observer<output_type> out) override {
    auto ptr = make_counted<buffer_sub<Trait>>(super::ctx_, max_items_, out);
    ptr->init(in_, select_);
    if (!ptr->running()) {
      auto err = ptr->err().or_else(sec::runtime_error,
                                    "failed to initialize buffer subscription");
      out.on_error(err);
      return {};
    }
    out.on_subscribe(subscription{ptr});
    return ptr->as_disposable();
  }

private:
  /// Configures how many items get pushed into one buffer at most.
  size_t max_items_;

  /// Sequence of input values.
  input in_;

  /// Sequence of control messages to select previous inputs.
  selector select_;
};

} // namespace caf::flow::op
