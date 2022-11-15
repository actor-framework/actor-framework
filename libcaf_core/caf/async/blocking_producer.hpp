// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/producer.hpp"
#include "caf/async/spsc_buffer.hpp"
#include "caf/detail/atomic_ref_counted.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/make_counted.hpp"

#include <condition_variable>
#include <optional>
#include <type_traits>

namespace caf::async {

/// Blocking interface for emitting items to an asynchronous consumer.
template <class T>
class blocking_producer {
public:
  class impl : public detail::atomic_ref_counted, public producer {
  public:
    impl() = delete;
    impl(const impl&) = delete;
    impl& operator=(const impl&) = delete;

    explicit impl(spsc_buffer_ptr<T> buf) : buf_(std::move(buf)) {
      buf_->set_producer(this);
    }

    bool push(span<const T> items) {
      std::unique_lock guard{mtx_};
      while (items.size() > 0) {
        while (demand_ == 0)
          cv_.wait(guard);
        if (demand_ < 0) {
          return false;
        } else {
          auto n = std::min(static_cast<size_t>(demand_), items.size());
          guard.unlock();
          buf_->push(items.subspan(0, n));
          guard.lock();
          demand_ -= static_cast<ptrdiff_t>(n);
          items = items.subspan(n);
        }
      }
      return true;
    }

    bool push(const T& item) {
      return push(make_span(&item, 1));
    }

    void close() {
      if (buf_) {
        buf_->close();
        buf_ = nullptr;
      }
    }

    void abort(error reason) {
      if (buf_) {
        buf_->abort(std::move(reason));
        buf_ = nullptr;
      }
    }

    bool canceled() const {
      std::unique_lock<std::mutex> guard{mtx_};
      return demand_ == -1;
    }

    void on_consumer_ready() override {
      // nop
    }

    void on_consumer_cancel() override {
      std::unique_lock<std::mutex> guard{mtx_};
      demand_ = -1;
      cv_.notify_all();
    }

    void on_consumer_demand(size_t demand) override {
      std::unique_lock<std::mutex> guard{mtx_};
      if (demand_ == 0) {
        demand_ += static_cast<ptrdiff_t>(demand);
        cv_.notify_all();
      } else if (demand_ > 0) {
        demand_ += static_cast<ptrdiff_t>(demand);
      }
    }

    void ref_producer() const noexcept override {
      ref();
    }

    void deref_producer() const noexcept override {
      deref();
    }

  private:
    spsc_buffer_ptr<T> buf_;
    mutable std::mutex mtx_;
    std::condition_variable cv_;
    ptrdiff_t demand_ = 0;
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

  /// Pushes an item to the consumer. If there is no demand by the consumer to
  /// deliver the item, this functions blocks unconditionally.
  /// @returns `true` if the item was delivered to the consumer or `false` if
  ///          the consumer no longer receives any additional item.
  bool push(const T& item) {
    return impl_->push(item);
  }

  /// Pushes multiple items to the consumer. If there is no demand by the
  /// consumer to deliver all items, this functions blocks unconditionally until
  /// all items have been delivered.
  /// @returns `true` if all items were delivered to the consumer or `false` if
  ///          the consumer no longer receives any additional item.
  bool push(span<const T> items) {
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

} // namespace caf::async
