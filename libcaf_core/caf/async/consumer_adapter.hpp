// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/consumer.hpp"
#include "caf/async/execution_context.hpp"
#include "caf/async/read_result.hpp"
#include "caf/async/spsc_buffer.hpp"
#include "caf/detail/atomic_ref_counted.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/make_counted.hpp"

#include <optional>

namespace caf::async {

/// Integrates an SPSC buffer consumer into an asynchronous event loop.
template <class T>
class consumer_adapter {
public:
  class impl : public detail::atomic_ref_counted, public consumer {
  public:
    impl() = delete;
    impl(const impl&) = delete;
    impl& operator=(const impl&) = delete;

    impl(spsc_buffer_ptr<T> buf, execution_context_ptr ctx, action do_wakeup)
      : buf_(std::move(buf)),
        ctx_(std::move(ctx)),
        do_wakeup_(std::move(do_wakeup)) {
      buf_->set_consumer(this);
    }

    ~impl() {
      if (buf_) {
        buf_->cancel();
        do_wakeup_.dispose();
      }
    }

    void cancel() {
      if (buf_) {
        buf_->cancel();
        buf_ = nullptr;
        do_wakeup_.dispose();
        do_wakeup_ = nullptr;
      }
    }

    template <class ErrorPolicy>
    read_result pull(ErrorPolicy policy, T& item) {
      if (buf_) {
        val_ = &item;
        auto [again, n] = buf_->pull(policy, 1u, *this);
        if (!again) {
          buf_ = nullptr;
        }
        if (n == 1) {
          return read_result::ok;
        } else if (again) {
          CAF_ASSERT(n == 0);
          return read_result::try_again_later;
        } else {
          CAF_ASSERT(n == 0);
          return abort_reason_ ? read_result::abort : read_result::stop;
        }
      } else {
        return abort_reason_ ? read_result::abort : read_result::stop;
      }
    }

    const error& abort_reason() const noexcept {
      return abort_reason_;
    }

    bool has_data() const noexcept {
      return buf_ ? buf_->has_data() : false;
    }

    bool has_consumer_event() const noexcept {
      return buf_ ? buf_->has_consumer_event() : false;
    }

    void on_next(const T& item) {
      *val_ = item;
    }

    void on_complete() {
      // nop
    }

    void on_error(const caf::error& what) {
      abort_reason_ = what;
    }

    void on_producer_ready() override {
      // nop
    }

    void on_producer_wakeup() override {
      ctx_->schedule(do_wakeup_);
    }

    void ref_consumer() const noexcept override {
      ref();
    }

    void deref_consumer() const noexcept override {
      deref();
    }

  private:
    spsc_buffer_ptr<T> buf_;
    execution_context_ptr ctx_;
    action do_wakeup_;
    error abort_reason_;
    T* val_ = nullptr;
  };

  using impl_ptr = intrusive_ptr<impl>;

  consumer_adapter() = default;

  consumer_adapter(const consumer_adapter&) = delete;

  consumer_adapter& operator=(const consumer_adapter&) = delete;

  consumer_adapter(consumer_adapter&&) = default;

  consumer_adapter& operator=(consumer_adapter&&) = default;

  explicit consumer_adapter(impl_ptr ptr) : impl_(std::move(ptr)) {
    // nop
  }

  ~consumer_adapter() {
    if (impl_)
      impl_->cancel();
  }

  template <class Policy>
  read_result pull(Policy policy, T& result) {
    if (impl_)
      return impl_->pull(policy, result);
    else
      return read_result::abort;
  }

  void cancel() {
    if (impl_) {
      impl_->cancel();
      impl_ = nullptr;
    }
  }

  error abort_reason() const noexcept {
    if (impl_)
      return impl_->abort_reason();
    else
      return make_error(sec::disposed);
  }

  explicit operator bool() const noexcept {
    return static_cast<bool>(impl_);
  }

  bool has_data() const noexcept {
    return impl_ ? impl_->has_data() : false;
  }

  bool has_consumer_event() const noexcept {
    return impl_ ? impl_->has_consumer_event() : false;
  }

  static consumer_adapter make(spsc_buffer_ptr<T> buf,
                               execution_context_ptr ctx, action do_wakeup) {
    if (buf) {
      using impl_t = typename consumer_adapter<T>::impl;
      auto impl = make_counted<impl_t>(std::move(buf), std::move(ctx),
                                       std::move(do_wakeup));
      return consumer_adapter<T>{std::move(impl)};
    } else {
      return {};
    }
  }

  static std::optional<consumer_adapter>
  make(consumer_resource<T> res, execution_context_ptr ctx, action do_wakeup) {
    if (auto buf = res.try_open()) {
      return {make(std::move(buf), std::move(ctx), std::move(do_wakeup))};
    } else {
      return {};
    }
  }

private:
  intrusive_ptr<impl> impl_;
};

/// @pre `buf != nullptr`
/// @relates consumer_adapter
template <class T>
consumer_adapter<T> make_consumer_adapter(spsc_buffer_ptr<T> buf,
                                          execution_context_ptr ctx,
                                          action do_wakeup) {
  return consumer_adapter<T>::make(std::move(buf), std::move(ctx),
                                   std::move(do_wakeup));
}

/// @relates consumer_adapter
template <class T>
std::optional<consumer_adapter<T>>
make_consumer_adapter(consumer_resource<T> res, execution_context_ptr ctx,
                      action do_wakeup) {
  if (auto buf = res.try_open()) {
    return {consumer_adapter<T>::make(std::move(buf), std::move(ctx),
                                      std::move(do_wakeup))};
  } else {
    return {};
  }
}

} // namespace caf::async
