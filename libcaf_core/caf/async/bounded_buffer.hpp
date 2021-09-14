// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstdlib>
#include <mutex>

#include "caf/async/consumer.hpp"
#include "caf/async/producer.hpp"
#include "caf/config.hpp"
#include "caf/defaults.hpp"
#include "caf/error.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/make_counted.hpp"
#include "caf/raise_error.hpp"
#include "caf/ref_counted.hpp"
#include "caf/sec.hpp"
#include "caf/span.hpp"
#include "caf/unit.hpp"

namespace caf::async {

/// Policy type for having `consume` call `on_error` immediately after the
/// producer has aborted even if the buffer still contains events.
struct prioritize_errors_t {
  static constexpr bool calls_on_error = true;
};

/// @relates prioritize_errors_t
constexpr auto prioritize_errors = prioritize_errors_t{};

/// Policy type for having `consume` call `on_error` only after processing all
/// events from the buffer.
struct delay_errors_t {
  static constexpr bool calls_on_error = true;
};

/// @relates delay_errors_t
constexpr auto delay_errors = delay_errors_t{};

/// Policy type for having `consume` treat errors as ordinary shutdowns.
struct ignore_errors_t {
  static constexpr bool calls_on_error = false;
};

/// @relates ignore_errors_t
constexpr auto ignore_errors = ignore_errors_t{};

/// A bounded buffer for transmitting events from one producer to one consumer.
template <class T>
class bounded_buffer : public ref_counted {
public:
  using value_type = T;

  bounded_buffer(uint32_t max_in_flight, uint32_t min_pull_size)
    : max_in_flight_(max_in_flight), min_pull_size_(min_pull_size) {
    buf_ = reinterpret_cast<T*>(malloc(sizeof(T) * max_in_flight * 2));
  }

  ~bounded_buffer() {
    auto first = buf_ + rd_pos_;
    auto last = buf_ + wr_pos_;
    std::destroy(first, last);
    free(buf_);
  }

  /// Appends to the buffer and calls `on_producer_wakeup` on the consumer if
  /// the buffer becomes non-empty.
  size_t push(span<const T> items) {
    std::unique_lock guard{mtx_};
    CAF_ASSERT(producer_ != nullptr);
    CAF_ASSERT(!closed_);
    CAF_ASSERT(wr_pos_ + items.size() < max_in_flight_ * 2);
    std::uninitialized_copy(items.begin(), items.end(), buf_ + wr_pos_);
    wr_pos_ += items.size();
    if (size() == items.size() && consumer_)
      consumer_->on_producer_wakeup();
    return capacity() - size();
  }

  size_t push(const T& item) {
    return push(make_span(&item, 1));
  }

  /// Consumes up to `demand` items from the buffer with `on_next`, ignoring any
  /// errors set by the producer.
  /// @tparam Policy Either `instant_error_t` or `delay_error_t`. Instant
  ///                error propagation requires also passing `on_error` handler.
  ///                Delaying errors without passing an `on_error` handler
  ///                effectively suppresses errors.
  /// @returns `true` if no more elements are available, `false` otherwise.
  template <class Policy, class OnNext, class OnError = unit_t>
  bool
  consume(Policy, size_t demand, OnNext on_next, OnError on_error = OnError{}) {
    static constexpr size_t local_buf_size = 16;
    if constexpr (Policy::calls_on_error)
      static_assert(!std::is_same_v<OnError, unit_t>,
                    "Policy requires an on_error handler");
    else
      static_assert(std::is_same_v<OnError, unit_t>,
                    "Policy prohibits an on_error handler");
    T local_buf[local_buf_size];
    std::unique_lock guard{mtx_};
    CAF_ASSERT(demand > 0);
    CAF_ASSERT(consumer_ != nullptr);
    if constexpr (std::is_same_v<Policy, prioritize_errors_t>) {
      if (err_) {
        on_error(err_);
        consumer_ = nullptr;
        return true;
      }
    }
    auto next_n = [this, &demand] {
      return std::min({local_buf_size, demand, size()});
    };
    for (auto n = next_n(); n > 0; n = next_n()) {
      auto first = buf_ + rd_pos_;
      std::move(first, first + n, local_buf);
      std::destroy(first, first + n);
      rd_pos_ += n;
      shift_elements();
      signal_demand(n);
      guard.unlock();
      on_next(make_span(local_buf, n));
      demand -= n;
      guard.lock();
    }
    if (!empty() || !closed_) {
      return false;
    } else {
      if constexpr (std::is_same_v<Policy, delay_errors_t>) {
        if (err_)
          on_error(err_);
      }
      consumer_ = nullptr;
      return true;
    }
  }

  /// Checks whether there is any pending data in the buffer.
  bool has_data() const noexcept {
    std::unique_lock guard{mtx_};
    return !empty();
  }

  /// Closes the buffer by request of the producer.
  void close() {
    std::unique_lock guard{mtx_};
    CAF_ASSERT(producer_ != nullptr);
    closed_ = true;
    producer_ = nullptr;
    if (empty() && consumer_)
      consumer_->on_producer_wakeup();
  }

  /// Closes the buffer and signals an error by request of the producer.
  void abort(error reason) {
    std::unique_lock guard{mtx_};
    closed_ = true;
    err_ = std::move(reason);
    producer_ = nullptr;
    if (empty() && consumer_) {
      consumer_->on_producer_wakeup();
      consumer_ = nullptr;
    }
  }

  /// Closes the buffer by request of the consumer.
  void cancel() {
    std::unique_lock guard{mtx_};
    if (producer_)
      producer_->on_consumer_cancel();
    consumer_ = nullptr;
  }

  void set_consumer(consumer_ptr consumer) {
    CAF_ASSERT(consumer != nullptr);
    std::unique_lock guard{mtx_};
    if (consumer_)
      CAF_RAISE_ERROR("producer-consumer queue already has a consumer");
    consumer_ = std::move(consumer);
    if (producer_)
      ready();
  }

  void set_producer(producer_ptr producer) {
    CAF_ASSERT(producer != nullptr);
    std::unique_lock guard{mtx_};
    if (producer_)
      CAF_RAISE_ERROR("producer-consumer queue already has a producer");
    producer_ = std::move(producer);
    if (consumer_)
      ready();
  }

  size_t capacity() const noexcept {
    return max_in_flight_;
  }

private:
  void ready() {
    producer_->on_consumer_ready();
    consumer_->on_producer_ready();
    if (!empty())
      consumer_->on_producer_wakeup();
    signal_demand(max_in_flight_);
  }

  size_t empty() const noexcept {
    CAF_ASSERT(wr_pos_ >= rd_pos_);
    return wr_pos_ == rd_pos_;
  }

  size_t size() const noexcept {
    CAF_ASSERT(wr_pos_ >= rd_pos_);
    return wr_pos_ - rd_pos_;
  }

  void shift_elements() {
    if (rd_pos_ >= max_in_flight_) {
      if (empty()) {
        rd_pos_ = 0;
        wr_pos_ = 0;
      } else {
        // No need to check for overlap: the first half of the buffer is
        // empty.
        auto first = buf_ + rd_pos_;
        auto last = buf_ + wr_pos_;
        std::uninitialized_move(first, last, buf_);
        std::destroy(first, last);
        wr_pos_ -= rd_pos_;
        rd_pos_ = 0;
      }
    }
  }

  void signal_demand(uint32_t new_demand) {
    demand_ += new_demand;
    if (demand_ >= min_pull_size_ && producer_) {
      producer_->on_consumer_demand(demand_);
      demand_ = 0;
    }
  }

  /// Guards access to all other member variables.
  mutable std::mutex mtx_;

  /// Allocated to max_in_flight_ * 2, but at most holds max_in_flight_
  /// elements at any point in time. We dynamically shift elements into the
  /// first half of the buffer whenever rd_pos_ crosses the midpoint.
  T* buf_;

  /// Stores how many items the buffer may hold at any time.
  uint32_t max_in_flight_;

  /// Configures the minimum amount of free buffer slots that we signal to the
  /// producer.
  uint32_t min_pull_size_;

  /// Stores the read position of the consumer.
  uint32_t rd_pos_ = 0;

  /// Stores the write position of the producer.
  uint32_t wr_pos_ = 0;

  /// Demand that has not yet been signaled back to the producer.
  uint32_t demand_ = 0;

  /// Stores whether `close` has been called.
  bool closed_ = false;

  /// Stores the abort reason.
  error err_;

  /// Callback handle to the consumer.
  consumer_ptr consumer_;

  /// Callback handle to the producer.
  producer_ptr producer_;
};

/// @relates bounded_buffer
template <class T>
using bounded_buffer_ptr = intrusive_ptr<bounded_buffer<T>>;

/// @relates bounded_buffer
template <class T, bool IsProducer>
struct resource_ctrl : ref_counted {
  using buffer_ptr = bounded_buffer_ptr<T>;

  explicit resource_ctrl(buffer_ptr ptr) : buf(std::move(ptr)) {
    // nop
  }

  ~resource_ctrl() {
    if (buf) {
      if constexpr (IsProducer) {
        auto err = make_error(sec::invalid_upstream,
                              "producer_resource destroyed without opening it");
        buf->abort(err);
      } else {
        buf->cancel();
      }
    }
  }

  buffer_ptr try_open() {
    std::unique_lock guard{mtx};
    if (buf) {
      auto res = buffer_ptr{};
      res.swap(buf);
      return res;
    }
    return nullptr;
  }

  mutable std::mutex mtx;
  buffer_ptr buf;
};

/// Grants read access to the first consumer that calls `open` on the resource.
/// Cancels consumption of items on the buffer if the resources gets destroyed
/// before opening it.
/// @relates bounded_buffer
template <class T>
class consumer_resource {
public:
  using value_type = T;

  using buffer_type = bounded_buffer<T>;

  using buffer_ptr = bounded_buffer_ptr<T>;

  explicit consumer_resource(buffer_ptr buf) {
    ctrl_.emplace(std::move(buf));
  }

  consumer_resource() = default;
  consumer_resource(consumer_resource&&) = default;
  consumer_resource(const consumer_resource&) = default;
  consumer_resource& operator=(consumer_resource&&) = default;
  consumer_resource& operator=(const consumer_resource&) = default;

  /// Tries to open the resource for reading from the buffer. The first `open`
  /// wins on concurrent access.
  /// @returns a pointer to the buffer on success, `nullptr` otherwise.
  buffer_ptr try_open() {
    if (ctrl_) {
      auto res = ctrl_->try_open();
      ctrl_ = nullptr;
      return res;
    } else {
      return nullptr;
    }
  }

private:
  intrusive_ptr<resource_ctrl<T, false>> ctrl_;
};

/// Grants access to a buffer to the first producer that calls `open`. Aborts
/// writes on the buffer if the resources gets destroyed before opening it.
/// @relates bounded_buffer
template <class T>
class producer_resource {
public:
  using value_type = T;

  using buffer_type = bounded_buffer<T>;

  using buffer_ptr = bounded_buffer_ptr<T>;

  explicit producer_resource(buffer_ptr buf) {
    ctrl_.emplace(std::move(buf));
  }

  producer_resource() = default;
  producer_resource(producer_resource&&) = default;
  producer_resource(const producer_resource&) = default;
  producer_resource& operator=(producer_resource&&) = default;
  producer_resource& operator=(const producer_resource&) = default;

  /// Tries to open the resource for writing to the buffer. The first `open`
  /// wins on concurrent access.
  /// @returns a pointer to the buffer on success, `nullptr` otherwise.
  buffer_ptr try_open() {
    if (ctrl_) {
      auto res = ctrl_->try_open();
      ctrl_ = nullptr;
      return res;
    } else {
      return nullptr;
    }
  }

private:
  intrusive_ptr<resource_ctrl<T, true>> ctrl_;
};

/// Creates bounded buffer and returns two resources connected by that buffer.
template <class T>
std::pair<consumer_resource<T>, producer_resource<T>>
make_bounded_buffer_resource(size_t buffer_size, size_t min_request_size) {
  using buffer_type = bounded_buffer<T>;
  auto buf = make_counted<buffer_type>(buffer_size, min_request_size);
  return {async::consumer_resource<T>{buf}, async::producer_resource<T>{buf}};
}

/// Creates bounded buffer and returns two resources connected by that buffer.
template <class T>
std::pair<consumer_resource<T>, producer_resource<T>>
make_bounded_buffer_resource() {
  return make_bounded_buffer_resource<T>(defaults::flow::buffer_size,
                                         defaults::flow::min_demand);
}

} // namespace caf::async
