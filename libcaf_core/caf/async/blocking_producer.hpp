// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/async/producer.hpp"
#include "caf/async/spsc_buffer.hpp"
#include "caf/async/write_result.hpp"
#include "caf/detail/atomic_ref_count.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/make_counted.hpp"

#include <condition_variable>
#include <optional>
#include <span>

namespace caf::async {

/// Blocking interface for emitting items to an asynchronous consumer.
template <class T>
class blocking_producer {
public:
  class impl : public producer {
  public:
    impl() = delete;
    impl(const impl&) = delete;
    impl& operator=(const impl&) = delete;

    explicit impl(spsc_buffer_ptr<T> buf) : buf_(std::move(buf)) {
      buf_->set_producer({this, add_ref});
    }

    bool push(std::span<const T> items) {
      // Only one thread can push at a time. The reason for this limitation is
      // that `try_push` can return `try_again_later`, which requires a matching
      // call to `on_consumer_demand` (or `on_consumer_cancel`) before we can
      // call `try_push` again.
      std::unique_lock push_guard{push_mtx_};
      if (!buf_) {
        return false;
      }
      while (!items.empty()) {
        const auto [n, wr] = buf_->try_push(items);
        switch (wr) {
          case write_result::ok:
            CAF_ASSERT(n == items.size());
            return true;
          case write_result::canceled:
            return false;
          default: { // write_result::try_again_later:
            CAF_ASSERT(wr == write_result::try_again_later);
            CAF_ASSERT(n < items.size());
            if (n > 0) {
              items = items.subspan(n);
            }
            // Wait until `on_consumer_cancel` changes the state to `canceled`
            // or `on_consumer_demand` changes the state to `notified`.
            // Note: if `try_push` returns `try_again_later`, then
            //       `on_consumer_demand` will be called with `unblocked = true`
            //       *once* (except when the consumer cancels).
            std::unique_lock guard{state_mtx_};
            while (state_ == state::idle) {
              state_cv_.wait(guard);
            }
            if (state_ == state::canceled) {
              return false;
            }
            CAF_ASSERT(state_ == state::notified);
            state_ = state::idle;
          }
        }
      }
      return true;
    }

    bool push(const T& item) {
      return push(std::span{&item, 1});
    }

    void close() {
      std::unique_lock guard{push_mtx_};
      if (buf_) {
        buf_->close();
        buf_ = nullptr;
      }
    }

    void abort(error reason) {
      std::unique_lock guard{push_mtx_};
      if (buf_) {
        buf_->abort(std::move(reason));
        buf_ = nullptr;
      }
    }

    bool canceled() const {
      std::unique_lock guard{state_mtx_};
      return state_ == state::canceled;
    }

    void on_consumer_ready() override {
      // nop
    }

    void on_consumer_cancel() override {
      std::unique_lock guard{state_mtx_};
      state_ = state::canceled;
      state_cv_.notify_one();
    }

    void on_consumer_demand(size_t, bool unblocked) override {
      if (unblocked) {
        std::unique_lock guard{state_mtx_};
        CAF_ASSERT(state_ == state::idle);
        state_ = state::notified;
        state_cv_.notify_one();
      }
    }

    void ref() const noexcept final {
      ref_count_.inc();
    }

    void deref() const noexcept final {
      ref_count_.dec(this);
    }

  private:
    enum class state { idle, notified, canceled };

    mutable detail::atomic_ref_count ref_count_;
    spsc_buffer_ptr<T> buf_;
    mutable std::mutex push_mtx_;
    mutable std::mutex state_mtx_;
    std::condition_variable state_cv_;
    state state_ = state::idle;
  };

  using impl_ptr = intrusive_ptr<impl>;

  blocking_producer() = default;

  blocking_producer(const blocking_producer&) = delete;

  blocking_producer& operator=(const blocking_producer&) = delete;

  blocking_producer(blocking_producer&&) = default;

  blocking_producer& operator=(blocking_producer&&) = default;

  explicit blocking_producer(impl_ptr ptr) : impl_(std::move(ptr)) {
    // nop
  }

  explicit blocking_producer(spsc_buffer_ptr<T> buf) {
    impl_.emplace(std::move(buf));
  }

  ~blocking_producer() {
    if (impl_)
      impl_->close();
  }

  /// Pushes an item to the consumer. If there is no room under the hard
  /// buffer limit, this function blocks until space is available or the
  /// consumer cancels.
  /// @returns `true` if the item was delivered to the buffer or `false` if
  ///          the consumer no longer receives any additional item.
  bool push(const T& item) {
    return impl_->push(item);
  }

  /// Pushes multiple items to the consumer, blocking on the hard buffer limit
  /// as needed.
  /// @returns `true` if all items were enqueued or `false` if the consumer
  ///          no longer receives any additional item.
  bool push(std::span<const T> items) {
    return impl_->push(items);
  }

  void close() {
    if (impl_) {
      impl_->close();
      impl_ = nullptr;
    }
  }

  void abort(error reason) {
    if (impl_) {
      impl_->abort(std::move(reason));
      impl_ = nullptr;
    }
  }

  /// Checks whether the consumer canceled its subscription.
  bool canceled() const {
    return impl_->canceled();
  }

  explicit operator bool() const noexcept {
    return static_cast<bool>(impl_);
  }

private:
  intrusive_ptr<impl> impl_;
};

/// @pre `buf != nullptr`
/// @relates blocking_producer
template <class T>
blocking_producer<T> make_blocking_producer(spsc_buffer_ptr<T> buf) {
  using impl_t = typename blocking_producer<T>::impl;
  return blocking_producer<T>{make_counted<impl_t>(std::move(buf))};
}

/// @relates blocking_producer
template <class T>
std::optional<blocking_producer<T>>
make_blocking_producer(producer_resource<T> res) {
  if (auto buf = res.try_open()) {
    return {make_blocking_producer(std::move(buf))};
  } else {
    return {};
  }
}

/// @relates blocking_producer
template <class T>
std::pair<blocking_producer<T>, consumer_resource<T>> make_blocking_producer() {
  auto [pull, push] = caf::async::make_spsc_buffer_resource<T>();
  return {make_blocking_producer(push.try_open()), std::move(pull)};
}

} // namespace caf::async
