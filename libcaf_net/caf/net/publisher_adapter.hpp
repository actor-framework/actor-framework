// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <memory>
#include <new>

#include "caf/async/publisher.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/subscription.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/socket_manager.hpp"

namespace caf::net {

template <class T>
class publisher_adapter final : public async::publisher<T>::impl,
                                public flow::subscription::impl {
public:
  publisher_adapter(socket_manager* owner, uint32_t max_in_flight,
                    uint32_t batch_size)
    : batch_size_(batch_size), max_in_flight_(max_in_flight), mgr_(owner) {
    CAF_ASSERT(max_in_flight > batch_size);
    buf_ = reinterpret_cast<T*>(malloc(sizeof(T) * max_in_flight * 2));
  }

  ~publisher_adapter() {
    auto first = buf_ + rd_pos_;
    auto last = buf_ + wr_pos_;
    std::destroy(first, last);
    free(buf_);
  }

  void subscribe(flow::observer<T> sink) override {
    if (std::unique_lock guard{mtx_}; !sink_) {
      sink_ = std::move(sink);
      auto ptr = intrusive_ptr<flow::subscription::impl>{this};
      sink_.on_attach(flow::subscription{std::move(ptr)});
    } else {
      sink.on_error(
        make_error(sec::downstream_already_exists,
                   "caf::net::publisher_adapter only allows one observer"));
    }
  }

  void request(size_t n) override {
    CAF_ASSERT(n > 0);
    // Reactive Streams specification 1.0.3:
    // > Subscription.request MUST place an upper bound on possible synchronous
    // > recursion between Publisher and Subscriber.
    std::unique_lock guard{mtx_};
    if (!sink_)
      return;
    credit_ += static_cast<uint32_t>(n);
    if (!in_request_body_) {
      in_request_body_ = true;
      auto n = std::min(size(), credit_);
      // When full, we take whatever we can out of the buffer even if the client
      // requests less than a batch. Otherwise, we try to wait until we have
      // sufficient credit for a full batch.
      if (n == 0) {
        in_request_body_ = false;
        return;
      } else if (full()) {
        wakeup();
      } else if (n < batch_size_) {
        in_request_body_ = false;
        return;
      }
      auto m = std::min(n, batch_size_);
      deliver(m);
      n -= m;
      while (sink_ && n >= batch_size_) {
        deliver(batch_size_);
        n -= batch_size_;
      }
      shift_elements();
      in_request_body_ = false;
    }
  }

  void cancel() override {
    std::unique_lock guard{mtx_};
    discard();
  }

  bool disposed() const noexcept override {
    std::unique_lock guard{mtx_};
    return mgr_ == nullptr;
  }

  void on_complete() {
    std::unique_lock guard{mtx_};
    if (sink_) {
      sink_.on_complete();
      sink_ = nullptr;
    }
  }

  void on_error(const error& what) {
    std::unique_lock guard{mtx_};
    if (sink_) {
      sink_.on_error(what);
      sink_ = nullptr;
    }
  }

  /// Enqueues a new element to the buffer.
  /// @returns The remaining buffer capacity. If this function return 0, the
  ///          manager MUST suspend reading until the observer consumes at least
  ///          one element.
  size_t push(T value) {
    std::unique_lock guard{mtx_};
    if (!mgr_)
      return 0;
    new (buf_ + wr_pos_) T(std::move(value));
    ++wr_pos_;
    if (auto n = std::min(size(), credit_); n >= batch_size_) {
      do {
        deliver(n);
        n -= batch_size_;
      } while (n >= batch_size_);
      shift_elements();
    }
    if (auto result = capacity(); result == 0 && credit_ > 0) {
      // Can only reach here if batch_size_ > credit_.
      deliver(credit_);
      shift_elements();
      return capacity();
    } else {
      return result;
    }
  }

  /// Pushes any buffered items to the observer as long as there is any
  /// available credit.
  void flush() {
    std::unique_lock guard{mtx_};
    while (sink_) {
      if (auto n = std::min({size(), credit_, batch_size_}); n > 0)
        deliver(n);
      else
        break;
    }
    shift_elements();
  }

private:
  void discard() {
    if (mgr_) {
      sink_ = nullptr;
      mgr_->mpx().discard(mgr_);
      mgr_ = nullptr;
      credit_ = 0;
    }
  }

  /// @pre `mtx_` is locked
  [[nodiscard]] uint32_t size() const noexcept {
    return wr_pos_ - rd_pos_;
  }

  /// @pre `mtx_` is locked
  [[nodiscard]] uint32_t capacity() const noexcept {
    return max_in_flight_ - size();
  }

  /// @pre `mtx_` is locked
  [[nodiscard]] bool full() const noexcept {
    return capacity() == 0;
  }

  /// @pre `mtx_` is locked
  [[nodiscard]] bool empty() const noexcept {
    return wr_pos_ == rd_pos_;
  }

  /// @pre `mtx_` is locked
  void wakeup() {
    CAF_ASSERT(mgr_ != nullptr);
    mgr_->mpx().register_reading(mgr_);
  }

  void deliver(uint32_t n) {
    auto first = buf_ + rd_pos_;
    auto last = first + n;
    CAF_ASSERT(rd_pos_ + n <= wr_pos_);
    rd_pos_ += n;
    CAF_ASSERT(credit_ >= n);
    credit_ -= n;
    sink_.on_next(span<const T>{first, n});
    std::destroy(first, last);
  }

  void shift_elements() {
    if (rd_pos_ >= max_in_flight_) {
      if (empty()) {
        rd_pos_ = 0;
        wr_pos_ = 0;
      } else {
        // No need to check for overlap: the first half of the buffer is empty.
        auto first = buf_ + rd_pos_;
        auto last = buf_ + wr_pos_;
        std::uninitialized_move(first, last, buf_);
        std::destroy(first, last);
        wr_pos_ -= rd_pos_;
        rd_pos_ = 0;
      }
    }
  }

  mutable std::recursive_mutex mtx_;

  /// Allocated to max_in_flight_ * 2, but at most holds max_in_flight_ elements
  /// at any point in time. We dynamically shift elements into the first half of
  /// the buffer whenever rd_pos_ crosses the midpoint.
  T* buf_;

  uint32_t rd_pos_ = 0;

  uint32_t wr_pos_ = 0;

  uint32_t credit_ = 0;

  uint32_t batch_size_;

  uint32_t max_in_flight_;

  bool in_request_body_ = false;

  flow::observer<T> sink_;

  intrusive_ptr<socket_manager> mgr_;
};

template <class T>
using publisher_adapter_ptr = intrusive_ptr<publisher_adapter<T>>;

} // namespace caf::net
