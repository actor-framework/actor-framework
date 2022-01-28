// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/consumer.hpp"
#include "caf/async/read_result.hpp"
#include "caf/async/spsc_buffer.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/make_counted.hpp"
#include "caf/ref_counted.hpp"

#include <chrono>
#include <condition_variable>
#include <type_traits>

namespace caf::async {

/// Blocking interface for emitting items to an asynchronous consumer.
template <class T>
class blocking_consumer {
public:
  class impl : public ref_counted, public consumer {
  public:
    impl() = delete;
    impl(const impl&) = delete;
    impl& operator=(const impl&) = delete;

    explicit impl(spsc_buffer_ptr<T> buf) : buf_(std::move(buf)) {
      buf_->set_consumer(this);
    }

    template <class ErrorPolicy, class TimePoint>
    read_result pull(ErrorPolicy policy, T& item, TimePoint timeout) {
      val_ = &item;
      std::unique_lock guard{buf_->mtx()};
      if constexpr (std::is_same_v<TimePoint, none_t>) {
        buf_->await_consumer_ready(guard, cv_);
      } else {
        if (!buf_->await_consumer_ready(guard, cv_, timeout))
          return read_result::timeout;
      }
      auto [again, n] = buf_->pull_unsafe(guard, policy, 1u, *this);
      CAF_IGNORE_UNUSED(again);
      CAF_ASSERT(!again || n == 1);
      if (n == 1) {
        return read_result::ok;
      } else if (buf_->abort_reason_unsafe()) {
        return read_result::abort;
      } else {
        return read_result::stop;
      }
    }

    template <class ErrorPolicy>
    read_result pull(ErrorPolicy policy, T& item) {
      return pull(policy, item, none);
    }

    void on_next(span<const T> items) {
      CAF_ASSERT(items.size() == 1);
      *val_ = items[0];
    }

    void on_complete() {
    }

    void on_error(const caf::error&) {
      // nop
    }

    void cancel() {
      if (buf_) {
        buf_->cancel();
        buf_ = nullptr;
      }
    }

    void on_producer_ready() override {
      // nop
    }

    void on_producer_wakeup() override {
      // NOTE: buf_->mtx() is already locked at this point
      cv_.notify_all();
    }

    void ref_consumer() const noexcept override {
      ref();
    }

    void deref_consumer() const noexcept override {
      deref();
    }

    error abort_reason() const {
      buf_->abort_reason()();
    }

    CAF_INTRUSIVE_PTR_FRIENDS(impl)

  private:
    spsc_buffer_ptr<T> buf_;
    std::condition_variable cv_;
    T* val_ = nullptr;
  };

  using impl_ptr = intrusive_ptr<impl>;

  blocking_consumer() = default;

  blocking_consumer(const blocking_consumer&) = delete;

  blocking_consumer& operator=(const blocking_consumer&) = delete;

  blocking_consumer(blocking_consumer&&) = default;

  blocking_consumer& operator=(blocking_consumer&&) = default;

  explicit blocking_consumer(impl_ptr ptr) : impl_(std::move(ptr)) {
    // nop
  }

  ~blocking_consumer() {
    if (impl_)
      impl_->cancel();
  }

  /// Fetches the next item. If there is no item available, this functions
  /// blocks unconditionally.
  /// @param policy Either @ref instant_error, @ref delay_error or
  ///               @ref ignore_errors.
  /// @param item Output parameter for storing the received item.
  /// @returns the status of the read operation. The function writes to `item`
  ///          only when also returning `read_result::ok`.
  template <class ErrorPolicy>
  read_result pull(ErrorPolicy policy, T& item) {
    return impl_->pull(policy, item);
  }

  /// Fetches the next item. If there is no item available, this functions
  /// blocks until the absolute timeout was reached.
  /// @param policy Either @ref instant_error, @ref delay_error or
  ///               @ref ignore_errors.
  /// @param item Output parameter for storing the received item.
  /// @param timeout Absolute timeout for the receive operation.
  /// @returns the status of the read operation. The function writes to `item`
  ///          only when also returning `read_result::ok`.
  template <class ErrorPolicy, class Clock,
            class Duration = typename Clock::duration>
  read_result pull(ErrorPolicy policy, T& item,
                   std::chrono::time_point<Clock, Duration> timeout) {
    return impl_->pull(policy, item, timeout);
  }

  /// Fetches the next item. If there is no item available, this functions
  /// blocks until the relative timeout was reached.
  /// @param policy Either @ref instant_error, @ref delay_error or
  ///               @ref ignore_errors.
  /// @param item Output parameter for storing the received item.
  /// @param timeout Maximum duration before returning from the function.
  /// @returns the status of the read operation. The function writes to `item`
  ///          only when also returning `read_result::ok`.
  template <class ErrorPolicy, class Rep, class Period>
  read_result pull(ErrorPolicy policy, T& item,
                   std::chrono::duration<Rep, Period> timeout) {
    auto abs_timeout = std::chrono::system_clock::now() + timeout;
    return impl_->pull(policy, item, abs_timeout);
  }

  error abort_reason() const {
    impl_->abort_reason();
  }

private:
  intrusive_ptr<impl> impl_;
};

template <class T>
expected<blocking_consumer<T>>
make_blocking_consumer(consumer_resource<T> res) {
  if (auto buf = res.try_open()) {
    using impl_t = typename blocking_consumer<T>::impl;
    return {blocking_consumer<T>{make_counted<impl_t>(std::move(buf))}};
  } else {
    return {make_error(sec::cannot_open_resource)};
  }
}

} // namespace caf::async
