#include "caf/flow/op/interval.hpp"

#include "caf/detail/plain_ref_counted.hpp"
#include "caf/flow/subscription.hpp"

#include <limits>
#include <utility>

namespace caf::flow::op {

class interval_action;

class interval_sub : public subscription::impl_base {
public:
  // -- constructors, destructors, and assignment operators --------------------

  interval_sub(coordinator* ctx, timespan initial_delay, timespan period,
               int64_t max_val, observer<int64_t> out)
    : ctx_(ctx),
      initial_delay_(initial_delay),
      period_(period),
      max_(max_val),
      out_(std::move(out)) {
    CAF_ASSERT(max_val > 0);
  }

  // -- implementation of subscription_impl ------------------------------------

  void dispose() override {
    if (out_) {
      ctx_->delay_fn([ptr = strong_this()] { ptr->do_cancel(); });
    }
  }

  bool disposed() const noexcept override {
    return !out_;
  }

  void request(size_t n) override {
    demand_ += n;
    if (!pending_) {
      if (val_ == 0)
        last_ = ctx_->steady_time() + initial_delay_;
      else
        last_ = ctx_->steady_time() + period_;
      schedule_next(last_);
    }
  }

  void schedule_next(coordinator::steady_time_point timeout) {
    pending_ = ctx_->delay_until(timeout,
                                 make_single_shot_action([this] { fire(); }));
  }

  void fire() {
    if (out_) {
      --demand_;
      out_.on_next(val_);
      if (++val_ == max_) {
        out_.on_complete();
        out_ = nullptr;
        pending_ = nullptr;
      } else if (demand_ > 0) {
        auto now = ctx_->steady_time();
        auto next = last_ + period_;
        while (next <= now)
          next += period_;
        last_ = next;
        schedule_next(next);
      } else {
        pending_ = nullptr;
      }
    }
  }

private:
  void do_cancel() {
    if (out_) {
      out_.on_complete();
      out_ = nullptr;
    }
    if (pending_) {
      pending_.dispose();
      pending_ = nullptr;
    }
  }

  intrusive_ptr<interval_sub> strong_this() {
    return intrusive_ptr<interval_sub>{this};
  }

  coordinator* ctx_;
  disposable pending_;
  timespan initial_delay_;
  timespan period_;
  coordinator::steady_time_point last_;
  int64_t val_ = 0;
  int64_t max_;
  size_t demand_ = 0;
  observer<int64_t> out_;
};

interval::interval(coordinator* ctx, timespan initial_delay, timespan period)
  : interval(ctx, initial_delay, period, std::numeric_limits<int64_t>::max()) {
  // nop
}

interval::interval(coordinator* ctx, timespan initial_delay, timespan period,
                   int64_t max_val)
  : super(ctx), initial_delay_(initial_delay), period_(period), max_(max_val) {
  // nop
}

disposable interval::subscribe(observer<int64_t> out) {
  // Intervals introduce a time dependency, so we need to watch them in order
  // to prevent actors from shutting down while timeouts are still pending.
  auto ptr = make_counted<interval_sub>(ctx_, initial_delay_, period_, max_,
                                        out);
  ctx_->watch(ptr->as_disposable());
  out.on_subscribe(subscription{ptr});
  return ptr->as_disposable();
}

} // namespace caf::flow::op
