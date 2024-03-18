// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/flow/op/interval.hpp"

#include "caf/detail/assert.hpp"
#include "caf/detail/plain_ref_counted.hpp"
#include "caf/flow/subscription.hpp"

#include <limits>
#include <utility>

namespace caf::flow::op {

class interval_action;

class interval_sub : public subscription::impl_base {
public:
  // -- constructors, destructors, and assignment operators --------------------

  interval_sub(coordinator* parent, timespan initial_delay, timespan period,
               int64_t max_val, observer<int64_t> out)
    : parent_(parent),
      initial_delay_(initial_delay),
      period_(period),
      max_(max_val),
      out_(std::move(out)) {
    CAF_ASSERT(max_val > 0);
  }

  // -- implementation of subscription_impl ------------------------------------

  coordinator* parent() const noexcept override {
    return parent_;
  }

  bool disposed() const noexcept override {
    return !out_;
  }

  void request(size_t n) override {
    demand_ += n;
    if (!pending_) {
      if (val_ == 0)
        last_ = parent_->steady_time() + initial_delay_;
      else
        last_ = parent_->steady_time() + period_;
      schedule_next(last_);
    }
  }

  void schedule_next(coordinator::steady_time_point timeout) {
    if (!out_) {
      // Ignore scheduled events after disposal.
      return;
    }
    pending_ = parent_->delay_until_fn(
      timeout, [sptr = intrusive_ptr<interval_sub>{this}] { //
        sptr->fire();
      });
  }

  void fire() {
    if (out_) {
      --demand_;
      out_.on_next(val_);
      if (++val_ == max_) {
        out_.on_complete();
        pending_ = nullptr;
      } else if (demand_ > 0) {
        auto now = parent_->steady_time();
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
  void do_dispose(bool from_external) override {
    if (!out_) {
      CAF_ASSERT(!pending_);
      return;
    }
    pending_.dispose();
    if (from_external)
      out_.on_error(error{sec::disposed});
    else
      out_.release_later();
  }

  coordinator* parent_;
  disposable pending_;
  timespan initial_delay_;
  timespan period_;
  coordinator::steady_time_point last_;
  int64_t val_ = 0;
  int64_t max_;
  size_t demand_ = 0;
  observer<int64_t> out_;
};

interval::interval(coordinator* parent, timespan initial_delay, timespan period)
  : interval(parent, initial_delay, period,
             std::numeric_limits<int64_t>::max()) {
  // nop
}

interval::interval(coordinator* parent, timespan initial_delay, timespan period,
                   int64_t max_val)
  : super(parent),
    initial_delay_(initial_delay),
    period_(period),
    max_(max_val) {
  // nop
}

disposable interval::subscribe(observer<int64_t> out) {
  // Intervals introduce a time dependency, so we need to watch them in order
  // to prevent actors from shutting down while timeouts are still pending.
  auto ptr = parent_->add_child(std::in_place_type<interval_sub>,
                                initial_delay_, period_, max_, out);
  parent_->watch(ptr->as_disposable());
  out.on_subscribe(subscription{ptr});
  return ptr->as_disposable();
}

} // namespace caf::flow::op
