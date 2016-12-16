/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_TEST_COORDINATOR_HPP
#define CAF_TEST_COORDINATOR_HPP

#include "caf/config.hpp"

#include <deque>
#include <chrono>
#include <limits>
#include <cstddef>
#include <algorithm>

#include "caf/scheduled_actor.hpp"
#include "caf/scheduler/abstract_coordinator.hpp"

namespace caf {
namespace scheduler {

/// A schedule coordinator for testing purposes.
class test_coordinator : public abstract_coordinator {
public:
  using super = abstract_coordinator;

  test_coordinator(actor_system& sys);

  /// A double-ended queue representing our current job queue.
  std::deque<resumable*> jobs;

  /// A scheduled message or timeout.
  struct delayed_msg {
    strong_actor_ptr from;
    strong_actor_ptr to;
    message_id mid;
    message msg;
  };

  /// A clock type using the highest available precision.
  using hrc = std::chrono::high_resolution_clock;

  /// A map type for storing scheduled messages and timeouts.
  std::multimap<hrc::time_point, delayed_msg> delayed_messages;

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

  /// Tries to execute a single event.
  bool run_once();

  /// Executes events until the job queue is empty and no pending timeouts are
  /// left. Returns the number of processed events.
  size_t run(size_t max_count = std::numeric_limits<size_t>::max());

  /// Tries to dispatch a single delayed message.
  bool dispatch_once();

  /// Dispatches all pending delayed messages. Returns the number of dispatched
  /// messages.
  size_t dispatch();

  /// Loops until no job or delayed message remains. Returns the total number of
  /// events (first) and dispatched delayed messages (second).
  std::pair<size_t, size_t> run_dispatch_loop();

protected:
  void start() override;

  void stop() override;

  void enqueue(resumable* ptr) override;
};

} // namespace scheduler
} // namespace caf

#endif // CAF_TEST_COORDINATOR_HPP

