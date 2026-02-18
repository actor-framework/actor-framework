// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/asynchronous_actor_clock.hpp"

#include "caf/action.hpp"
#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/log/core.hpp"
#include "caf/telemetry/gauge.hpp"
#include "caf/thread_owner.hpp"
#include "caf/timespan.hpp"

#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

namespace caf::detail {

namespace {

class default_actor_clock : public asynchronous_actor_clock {
public:
  struct entry {
    time_point timeout;
    caf::action callback;
  };

  /// Comparator for min-heap (smallest timeout at front).
  static constexpr auto entry_less = [](const entry& lhs, const entry& rhs) {
    return lhs.timeout > rhs.timeout;
  };

  using lock_type = std::unique_lock<std::mutex>;

  explicit default_actor_clock(telemetry::int_gauge* queue_size)
    : queue_size_(queue_size) {
    CAF_ASSERT(queue_size != nullptr);
  }

  ~default_actor_clock() override {
    stop();
  }

  void start(caf::actor_system& sys) override {
    CAF_ASSERT(!worker_.joinable());
    auto cleanup_interval = get_or(sys.config(), "caf.clock.cleanup-interval",
                                   timespan::zero());
    log::core::info("starting the default actor clock with cleanup interval {}",
                    cleanup_interval);
    worker_
      = sys.launch_thread("caf.clock", caf::thread_owner::system,
                          [this, cleanup_interval] { run(cleanup_interval); });
  }

  void stop() override {
    if (worker_.joinable()) {
      auto stop_helper = make_single_shot_action([this] {
        std::vector<entry> queue;
        {
          lock_type guard{mutex_};
          stopped_ = true;
          queue.swap(queue_);
        }
        if (auto dropped = static_cast<int64_t>(queue.size()); dropped > 0) {
          for (auto& entry : queue) {
            entry.callback.dispose();
          }
          queue_size_->dec(dropped);
        }
      });
      {
        lock_type guard{mutex_};
        push({time_point::min(), std::move(stop_helper)});
      }
      cv_.notify_one();
      worker_.join();
      log::core::info("stopped the default actor clock");
      worker_ = std::thread{};
    }
  }

  caf::disposable schedule(time_point timeout, caf::action callback) override {
    if (!callback) {
      return {};
    }
    // Only wake up the dispatcher if the new timeout is smaller than the
    // current timeout.
    auto added = true;
    auto do_wakeup = false;
    {
      lock_type guard{mutex_};
      if (!stopped_) {
        do_wakeup = queue_.empty() || timeout < queue_.front().timeout;
        push({timeout, callback});
      } else {
        added = false;
      }
    }
    if (!added) {
      log::core::debug("schedule: clock is stopped, disposing callback");
      callback.dispose();
      return {};
    }
    if (do_wakeup) {
      cv_.notify_one();
    }
    return std::move(callback).as_disposable();
  }

  void run(timespan cleanup_interval) {
    lock_type guard{mutex_};
    auto await_non_empty = [this, &guard] {
      while (queue_.empty()) {
        cv_.wait(guard);
      }
    };
    auto initial_cleanup_time = [this, cleanup_interval] {
      if (cleanup_interval.count() > 0) {
        return now() + cleanup_interval;
      } else {
        return time_point::max();
      }
    };
    auto next_cleanup = initial_cleanup_time();
    await_non_empty();
    for (;;) {
      time_point job_timeout;
      if (run_next(guard, now(), job_timeout)) {
        if (stopped_) {
          return;
        }
        await_non_empty();
      } else {
        auto next_wakeup = std::min(job_timeout, next_cleanup);
        cv_.wait_until(guard, next_wakeup);
      }
      if (auto current_time = now(); current_time >= next_cleanup) {
        cleanup();
        next_cleanup = current_time + cleanup_interval;
        await_non_empty();
      }
    }
  }

  /// @pre `mutex_` is locked
  void push(entry e) {
    queue_size_->inc();
    queue_.push_back(std::move(e));
    std::push_heap(queue_.begin(), queue_.end(), entry_less);
  }

  /// @pre `mutex_` is locked
  void pop() {
    queue_size_->dec();
    std::pop_heap(queue_.begin(), queue_.end(), entry_less);
    queue_.pop_back();
  }

  /// @pre `mutex_` is locked
  /// @returns `true` if a job was run, `false` otherwise.
  bool run_next(lock_type& guard, time_point current_time,
                time_point& job_timeout) {
    CAF_ASSERT(!queue_.empty());
    auto& job = queue_.front();
    CAF_ASSERT(job.callback.ptr() != nullptr);
    job_timeout = job.timeout;
    if (current_time >= job_timeout) {
      auto fn = std::move(job.callback);
      pop();
      guard.unlock();
      fn.run();
      guard.lock();
      return true;
    }
    return false;
  }

  /// @pre `mutex_` is locked
  void cleanup() {
    // n = number of entries in the heap
    // k = number of disposed entries
    //
    // To implement the cleanup, we have the choice between: (1) remove each
    // entry individually in O(k log n) while going down the heap; or (2)
    // partition-then-rebuild the heap in O(n).
    //
    // If n is very large and k is small, (1) can be faster. However, (2) will
    // be faster for large k because (1) will approach O(n log n) while (2) is
    // always O(n).
    //
    // For now, we pick partition-then-rebuild because it avoids the O(n log n)
    // worst case performance.
    auto is_disposed = [](const entry& e) { return e.callback.disposed(); };
    if (auto erased = std::erase_if(queue_, is_disposed); erased > 0) {
      queue_size_->dec(static_cast<int64_t>(erased));
      log::core::debug("cleanup removed {} disposed entries from the clock",
                       erased);
      std::make_heap(queue_.begin(), queue_.end(), entry_less);
    }
  }

private:
  /// Tracks the number of entries in the actor clock queue.
  telemetry::int_gauge* queue_size_;

  /// Guards access to the stopped_ and queue_ member variables.
  std::mutex mutex_;

  /// Signals the worker thread to wake up.
  std::condition_variable cv_;

  /// Tracks whether `stop` has been called.
  bool stopped_ = false;

  /// The queue of entries to be executed.
  std::vector<entry> queue_;

  /// The worker thread that runs the clock.
  std::thread worker_;
};

} // namespace

asynchronous_actor_clock::~asynchronous_actor_clock() {
  // nop
}

std::unique_ptr<asynchronous_actor_clock>
asynchronous_actor_clock::make(telemetry::int_gauge* queue_size) {
  return std::make_unique<default_actor_clock>(queue_size);
}

} // namespace caf::detail
