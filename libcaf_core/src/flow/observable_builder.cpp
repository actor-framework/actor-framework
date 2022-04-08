// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/flow/observable_builder.hpp"

#include <limits>

namespace caf::flow {

class interval_action : public ref_counted, public action::impl {
public:
  interval_action(intrusive_ptr<interval_impl> impl)
    : state_(action::state::scheduled), impl_(std::move(impl)) {
    // nop
  }

  void dispose() override {
    state_ = action::state::disposed;
  }

  bool disposed() const noexcept override {
    return state_.load() == action::state::disposed;
  }

  action::state current_state() const noexcept override {
    return state_.load();
  }

  void run() override {
    if (state_.load() == action::state::scheduled)
      impl_->fire(this);
  }

  void ref_disposable() const noexcept override {
    ref();
  }

  void deref_disposable() const noexcept override {
    deref();
  }

  friend void intrusive_ptr_add_ref(const interval_action* ptr) noexcept {
    ptr->ref();
  }

  friend void intrusive_ptr_release(const interval_action* ptr) noexcept {
    ptr->deref();
  }

private:
  std::atomic<action::state> state_;
  intrusive_ptr<interval_impl> impl_;
};

interval_impl::interval_impl(coordinator* ctx, timespan initial_delay,
                             timespan period)
  : interval_impl(ctx, initial_delay, period,
                  std::numeric_limits<int64_t>::max()) {
  // nop
}

interval_impl::interval_impl(coordinator* ctx, timespan initial_delay,
                             timespan period, int64_t max_val)
  : super(ctx), initial_delay_(initial_delay), period_(period), max_(max_val) {
  CAF_ASSERT(max_val > 0);
}

void interval_impl::dispose() {
  if (obs_) {
    obs_.on_complete();
    obs_ = nullptr;
  }
  if (pending_) {
    pending_.dispose();
    pending_ = nullptr;
  }
  val_ = max_;
}

bool interval_impl::disposed() const noexcept {
  return val_ == max_;
}

void interval_impl::on_request(disposable_impl* ptr, size_t n) {
  if (obs_.ptr() == ptr) {
    if (demand_ == 0 && !pending_) {
      if (val_ == 0)
        last_ = ctx_->steady_time() + initial_delay_;
      else
        last_ = ctx_->steady_time() + period_;
      pending_ = ctx_->delay_until(last_,
                                   action{make_counted<interval_action>(this)});
    }
    demand_ += n;
  }
}

void interval_impl::on_cancel(disposable_impl* ptr) {
  if (obs_.ptr() == ptr) {
    obs_ = nullptr;
    pending_.dispose();
    val_ = max_;
  }
}

disposable interval_impl::subscribe(observer<int64_t> sink) {
  if (obs_ || val_ == max_) {
    sink.on_error(make_error(sec::invalid_observable));
    return {};
  } else {
    obs_ = sink;
    return super::do_subscribe(sink.ptr());
  }
}

void interval_impl::fire(interval_action* act) {
  if (obs_) {
    --demand_;
    obs_.on_next(make_span(&val_, 1));
    if (++val_ == max_) {
      obs_.on_complete();
      obs_ = nullptr;
      pending_ = nullptr;
    } else if (demand_ > 0) {
      auto now = ctx_->steady_time();
      auto next = last_ + period_;
      while (next <= now)
        next += period_;
      last_ = next;
      pending_ = ctx_->delay_until(next, action{act});
    }
  }
}

} // namespace caf::flow
