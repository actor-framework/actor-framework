// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <list>
#include <mutex>
#include <tuple>

#include "caf/async/batch.hpp"
#include "caf/defaults.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/subscription.hpp"

namespace caf::async {

/// Enables buffered consumption of published items.
template <class T>
class observer_buffer : public flow::observer_impl<T> {
public:
  observer_buffer() {
    // nop
  }

  void on_complete() override {
    std::unique_lock guard{mtx_};
    if (!done_) {
      sub_ = nullptr;
      done_ = true;
      deinit(guard);
    }
  }

  void on_error(const error& what) override {
    std::unique_lock guard{mtx_};
    if (!done_) {
      sub_ = nullptr;
      done_ = true;
      err_ = what;
      deinit(guard);
    }
  }

  void on_next(span<const T> items) override {
    on_batch(make_batch(items));
  }

  void on_batch(const batch& buf) override {
    std::list<batch> ls;
    ls.emplace_back(buf);
    std::unique_lock guard{mtx_};
    batches_.splice(batches_.end(), std::move(ls));
    if (batches_.size() == 1)
      wakeup(guard);
  }

  void on_attach(flow::subscription sub) override {
    CAF_ASSERT(sub.valid());
    std::unique_lock guard{mtx_};
    sub_ = std::move(sub);
    init(guard);
  }

  bool has_data() {
    if (local.pos_ != local.end_) {
      return true;
    } else {
      std::unique_lock guard{mtx_};
      return !batches_.empty();
    }
  }

  /// Tries to fetch the next value. If no value exists, the first element in
  /// the tuple is `nullptr`. The second value indicates whether the stream was
  /// closed. If the stream was closed, the third value is `nullptr` if
  /// `on_complete` was called and a pointer to the error if `on_error` was
  /// called.
  std::tuple<const T*, bool, const error*> poll() {
    if (local.pos_ != local.end_) {
      auto res = local.pos_;
      if (++local.pos_ == local.end_) {
        std::unique_lock guard{mtx_};
        if (sub_)
          sub_.request(local.cache_.size());
      }
      return {res, false, nullptr};
    } else if (std::unique_lock guard{mtx_}; !batches_.empty()) {
      batches_.front().swap(local.cache_);
      batches_.pop_front();
      guard.unlock();
      auto items = local.cache_.template items<T>();
      CAF_ASSERT(!items.empty());
      local.pos_ = items.begin();
      local.end_ = items.end();
      return {local.pos_++, false, nullptr};
    } else if (!err_) {
      return {nullptr, done_, nullptr};
    } else {
      return {nullptr, true, &err_};
    }
  }

  void dispose() override {
    on_complete();
  }

  bool disposed() const noexcept override {
    std::unique_lock guard{mtx_};
    return done_;
  }

protected:
  template <class WaitFn>
  std::tuple<const T*, bool, const error*> wait_with(WaitFn wait_fn) {
    if (local.pos_ != local.end_) {
      auto res = local.pos_;
      if (++local.pos_ == local.end_) {
        std::unique_lock guard{mtx_};
        if (sub_)
          sub_.request(local.cache_.size());
      }
      return {res, false, nullptr};
    } else {
      std::unique_lock guard{mtx_};
      while (batches_.empty() && !done_)
        wait_fn(guard);
      if (!batches_.empty()) {
        batches_.front().swap(local.cache_);
        batches_.pop_front();
        guard.unlock();
        auto items = local.cache_.template items<T>();
        local.pos_ = items.begin();
        local.end_ = items.end();
        return {local.pos_++, false, nullptr};
      } else if (!err_) {
        return {nullptr, done_, nullptr};
      } else {
        return {nullptr, true, &err_};
      }
    }
  }
  /// Wraps fields that we only access from the consumer's thread.
  struct local_t {
    const T* pos_ = nullptr;
    const T* end_ = nullptr;
    batch cache_;
  } local;

  static_assert(sizeof(local_t) < CAF_CACHE_LINE_SIZE);

  /// Avoids false sharing.
  char pad_[CAF_CACHE_LINE_SIZE - sizeof(local_t)];

  // -- state for consumer and publisher ---------------------------------------

  /// Protects fields that we access with both the consumer and the producer.
  mutable std::mutex mtx_;
  flow::subscription sub_;
  bool done_ = false;
  error err_;
  std::list<batch> batches_;

private:
  virtual void init(std::unique_lock<std::mutex>&) {
    sub_.request(defaults::flow::buffer_size);
  }

  virtual void deinit(std::unique_lock<std::mutex>& guard) {
    wakeup(guard);
  }

  virtual void wakeup(std::unique_lock<std::mutex>&) {
    // Customization point.
  }
};

} // namespace caf::async
