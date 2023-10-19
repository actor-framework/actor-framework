// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/atomic_ref_counted.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/op/hot.hpp"
#include "caf/flow/subscription.hpp"
#include "caf/sec.hpp"

#include <utility>

namespace caf::flow::op {

/// Reads from an observable buffer and emits the consumed items.
template <class Buffer>
class from_resource_sub : public detail::atomic_ref_counted,
                          public subscription::impl,
                          public async::consumer {
public:
  // -- member types -----------------------------------------------------------

  using value_type = typename Buffer::value_type;

  using buffer_ptr = intrusive_ptr<Buffer>;

  // -- constructors, destructors, and assignment operators --------------------

  from_resource_sub(coordinator* ctx, buffer_ptr buf, observer<value_type> out)
    : ctx_(ctx), buf_(buf), out_(std::move(out)) {
    ctx_->ref_execution_context();
  }

  ~from_resource_sub() {
    ctx_->deref_execution_context();
  }

  // -- implementation of subscription_impl ------------------------------------

  bool disposed() const noexcept override {
    return disposed_;
  }

  void dispose() override {
    if (!disposed_) {
      disposed_ = true;
      if (!running_)
        do_dispose();
    }
  }

  void request(size_t n) override {
    if (demand_ != 0) {
      demand_ += n;
    } else {
      demand_ = n;
      run_later();
    }
  }

  // -- implementation of async::consumer: these may get called concurrently ---

  void on_producer_ready() override {
    // nop
  }

  void on_producer_wakeup() override {
    ctx_->schedule_fn([ptr = strong_this()] {
      ptr->running_ = true;
      ptr->do_run();
    });
  }

  // -- intrusive_ptr interface ------------------------------------------------

  friend void intrusive_ptr_add_ref(const from_resource_sub* ptr) noexcept {
    ptr->ref();
  }

  friend void intrusive_ptr_release(const from_resource_sub* ptr) noexcept {
    ptr->deref();
  }

  void ref_consumer() const noexcept final {
    this->ref();
  }

  void deref_consumer() const noexcept final {
    this->deref();
  }

  void ref_disposable() const noexcept final {
    this->ref();
  }

  void deref_disposable() const noexcept final {
    this->deref();
  }

private:
  void run_later() {
    if (!running_) {
      running_ = true;
      ctx_->delay_fn([ptr = strong_this()] { ptr->do_run(); });
    }
  }

  void do_dispose() {
    if (buf_) {
      auto tmp = std::move(buf_);
      tmp->cancel();
    }
    if (out_) {
      auto tmp = std::move(out_);
      tmp.on_complete();
    }
  }

  void do_run() {
    auto guard = detail::make_scope_guard([this] { running_ = false; });
    if (disposed_) {
      do_dispose();
      return;
    }
    CAF_ASSERT(out_);
    CAF_ASSERT(buf_);
    while (demand_ > 0) {
      auto [again, pulled] = buf_->pull(async::delay_errors, demand_, out_);
      if (!again) {
        buf_ = nullptr;
        out_ = nullptr;
        disposed_ = true;
        return;
      } else if (disposed_) {
        do_dispose();
        return;
      } else if (pulled == 0) {
        return;
      } else {
        CAF_ASSERT(demand_ >= pulled);
        demand_ -= pulled;
      }
    }
  }

  intrusive_ptr<from_resource_sub> strong_this() {
    return {this};
  }

  /// Stores the @ref coordinator that runs this flow. Unlike other observables,
  /// we need a strong reference to the coordinator because otherwise the buffer
  /// might call `schedule_fn` on a destroyed object.
  intrusive_ptr<coordinator> ctx_;

  /// Stores a pointer to the asynchronous input buffer.
  buffer_ptr buf_;

  /// Stores a pointer to the target observer running on `remote_ctx_`.
  observer<value_type> out_;

  /// Stores whether do_run is currently running.
  bool running_ = false;

  /// Stores whether dispose() has been called.
  bool disposed_ = false;

  /// Stores the demand from the observer.
  size_t demand_ = 0;
};

template <class T>
class from_resource : public hot<T> {
public:
  // -- member types -----------------------------------------------------------

  using resource_type = async::consumer_resource<T>;

  using super = hot<T>;

  // -- constructors, destructors, and assignment operators --------------------

  from_resource(coordinator* ctx, resource_type resource)
    : super(ctx), resource_(std::move(resource)) {
    // nop
  }

  // -- implementation of observable_impl<T> -----------------------------------

  disposable subscribe(observer<T> out) override {
    CAF_ASSERT(out);
    if (resource_) {
      if (auto buf = resource_.try_open()) {
        resource_ = nullptr;
        using buffer_type = typename resource_type::buffer_type;
        using impl_t = from_resource_sub<buffer_type>;
        auto ptr = make_counted<impl_t>(super::ctx_, buf, out);
        buf->set_consumer(ptr);
        super::ctx_->watch(ptr->as_disposable());
        out.on_subscribe(subscription{ptr});
        return ptr->as_disposable();
      } else {
        resource_ = nullptr;
        auto err = make_error(sec::cannot_open_resource,
                              "failed to open an async resource");
        out.on_error(err);
        return disposable{};
      }
    } else {
      auto err = make_error(sec::too_many_observers,
                            "may only subscribe once to an async resource");
      out.on_error(err);
      return disposable{};
    }
  }

private:
  resource_type resource_;
};

} // namespace caf::flow::op
