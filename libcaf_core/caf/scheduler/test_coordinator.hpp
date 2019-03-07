/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include <deque>
#include <chrono>
#include <limits>
#include <cstddef>
#include <algorithm>

#include "caf/config.hpp"
#include "caf/detail/test_actor_clock.hpp"
#include "caf/raise_error.hpp"
#include "caf/scheduled_actor.hpp"
#include "caf/scheduler/abstract_coordinator.hpp"

namespace caf {
namespace scheduler {

/// A schedule coordinator for testing purposes.
class test_coordinator : public abstract_coordinator {
public:
  using super = abstract_coordinator;

  /// A type-erased boolean predicate.
  using bool_predicate = std::function<bool()>;

  test_coordinator(actor_system& sys);

  /// A double-ended queue representing our current job queue.
  std::deque<resumable*> jobs;

  /// Returns whether at least one job is in the queue.
  inline bool has_job() const {
    return !jobs.empty();
  }

  /// Returns the next element from the job queue as type `T`.
  template <class T = resumable>
  T& next_job() {
    if (jobs.empty())
      CAF_RAISE_ERROR("jobs.empty()");
    return dynamic_cast<T&>(*jobs.front());
  }

  /// Peeks into the mailbox of `next_job<scheduled_actor>()`.
  template <class T>
  const T& peek() {
    auto ptr = next_job<scheduled_actor>().mailbox().peek();
    CAF_ASSERT(ptr != nullptr);
    if (!ptr->content().match_elements<T>())
      CAF_RAISE_ERROR("Mailbox element does not match T.");
    return ptr->content().get_as<T>(0);
  }

  /// Puts `x` at the front of the queue unless it cannot be found in the queue.
  /// Returns `true` if `x` exists in the queue and was put in front, `false`
  /// otherwise.
  template <class Handle>
  bool prioritize(const Handle& x) {
    auto ptr = dynamic_cast<resumable*>(actor_cast<abstract_actor*>(x));
    if (!ptr)
      return false;
    auto b = jobs.begin();
    auto e = jobs.end();
    auto i = std::find(b, e, ptr);
    if (i == e)
      return false;
    if (i == b)
      return true;
    std::rotate(b, i, i + 1);
    return true;
  }

  /// Runs all jobs that satisfy the predicate.
  template <class Predicate>
  size_t run_jobs_filtered(Predicate predicate) {
    size_t result = 0;
    while (!jobs.empty()) {
      auto b = jobs.begin();
      auto e = jobs.end();
      auto i = std::find_if(b, e, predicate);
      if (i == e)
        return result;
      if (i != b)
        std::rotate(b, i, i + 1);
      run_once();
      ++result;
    }
    return result;
  }

  /// Tries to execute a single event in FIFO order.
  bool try_run_once();

  /// Tries to execute a single event in LIFO order.
  bool try_run_once_lifo();

  /// Executes a single event in FIFO order or fails if no event is available.
  void run_once();

  /// Executes a single event in LIFO order or fails if no event is available.
  void run_once_lifo();

  /// Executes events until the job queue is empty and no pending timeouts are
  /// left. Returns the number of processed events.
  size_t run(size_t max_count = std::numeric_limits<size_t>::max());

  /// Returns whether at least one pending timeout exists.
  bool has_pending_timeout() const {
    return clock_.has_pending_timeout();
  }

  /// Tries to trigger a single timeout.
  bool trigger_timeout() {
    return clock_.trigger_timeout();
  }

  /// Triggers all pending timeouts.
  size_t trigger_timeouts() {
    return clock_.trigger_timeouts();
  }

  /// Advances simulation time and returns the number of triggered timeouts.
  size_t advance_time(timespan x) {
    return clock_.advance_time(x);
  }

  template <class F>
  void after_next_enqueue(F f) {
    after_next_enqueue_ = f;
  }

  /// Executes the next enqueued job immediately by using the
  /// `after_next_enqueue` hook.
  void inline_next_enqueue();

  /// Executes all enqueued jobs immediately by using the `after_next_enqueue`
  /// hook.
  void inline_all_enqueues();

  bool detaches_utility_actors() const override;

  detail::test_actor_clock& clock() noexcept override;

protected:
  void start() override;

  void stop() override;

  void enqueue(resumable* ptr) override;

private:
  void inline_all_enqueues_helper();

  /// Allows users to fake time at will.
  detail::test_actor_clock clock_;

  /// User-provided callback for triggering custom code in `enqueue`.
  std::function<void()> after_next_enqueue_;
};

} // namespace scheduler
} // namespace caf
