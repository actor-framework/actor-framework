// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/cow_vector.hpp"
#include "caf/flow/observable_decl.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/op/cold.hpp"
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

  // -- callbacks for the parent -----------------------------------------------

  void init(observable<input_type> vals, observable<select_token_type> ctrl) {
    using val_fwd_t = forwarder<input_type, buffer_sub, buffer_input_t>;
    using ctrl_fwd_t = forwarder<select_token_type, buffer_sub, buffer_emit_t>;
    vals.subscribe(
      make_counted<val_fwd_t>(this, buffer_input_t{})->as_observer());
    ctrl.subscribe(
      make_counted<ctrl_fwd_t>(this, buffer_emit_t{})->as_observer());
  }

  // -- callbacks for the forwarders -------------------------------------------

  void fwd_on_subscribe(buffer_input_t, subscription sub) {
    if (!value_sub_ && out_) {
      value_sub_ = std::move(sub);
      if (pending_demand_ > 0) {
        value_sub_.request(pending_demand_);
        pending_demand_ = 0;
      }
    } else {
      sub.dispose();
    }
  }

  void fwd_on_complete(buffer_input_t) {
    CAF_ASSERT(value_sub_.valid());
    CAF_ASSERT(out_.valid());
    value_sub_ = nullptr;
    if (!buf_.empty())
      do_emit();
    out_.on_complete();
    out_ = nullptr;
    if (control_sub_) {
      control_sub_.dispose();
      control_sub_ = nullptr;
    }
  }

  void fwd_on_error(buffer_input_t, const error& what) {
    value_sub_ = nullptr;
    do_abort(what);
  }

  void fwd_on_next(buffer_input_t, const input_type& item) {
    buf_.push_back(item);
    if (buf_.size() == max_buf_size_)
      do_emit();
  }

  void fwd_on_subscribe(buffer_emit_t, subscription sub) {
    if (!control_sub_ && out_) {
      control_sub_ = std::move(sub);
      control_sub_.request(1);
    } else {
      sub.dispose();
    }
  }

  void fwd_on_complete(buffer_emit_t) {
    do_abort(make_error(sec::end_of_stream,
                        "buffer: unexpected end of the control stream"));
  }

  void fwd_on_error(buffer_emit_t, const error& what) {
    control_sub_ = nullptr;
    do_abort(what);
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
    if (value_sub_)
      value_sub_.request(n);
    else
      pending_demand_ += n;
  }

private:
  void do_emit() {
    Trait f;
    out_.on_next(f(buf_));
    buf_.clear();
  }

  void do_dispose() {
    if (value_sub_) {
      value_sub_.dispose();
      value_sub_ = nullptr;
    }
    if (control_sub_) {
      control_sub_.dispose();
      control_sub_ = nullptr;
    }
    if (out_) {
      out_.on_complete();
    }
  }

  void do_abort(const error& reason) {
    if (value_sub_) {
      value_sub_.dispose();
      value_sub_ = nullptr;
    }
    if (control_sub_) {
      control_sub_.dispose();
      control_sub_ = nullptr;
    }
    if (out_) {
      out_.on_error(reason);
    }
  }

  /// Stores the context (coordinator) that runs this flow.
  coordinator* ctx_;

  /// Stores the maximum buffer size before forcing a batch.
  size_t max_buf_size_;

  /// Stores the elements until we can emit them.
  std::vector<input_type> buf_;

  /// Stores a handle to the subscribed observer.
  observer<output_type> out_;

  /// Our subscription for the values.
  subscription value_sub_;

  /// Our subscription for the control tokens.
  subscription control_sub_;

  ///
  size_t pending_demand_ = 0;
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
