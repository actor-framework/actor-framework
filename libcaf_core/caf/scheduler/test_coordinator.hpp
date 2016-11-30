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
#include <cstddef>

#include "caf/scheduler/abstract_coordinator.hpp"

namespace caf {
namespace scheduler {

/// Coordinator for testing purposes.
class test_coordinator : public abstract_coordinator {
public:
  using super = abstract_coordinator;

  test_coordinator(actor_system& sys);

  std::deque<resumable*> jobs;

  struct delayed_msg {
    strong_actor_ptr from;
    strong_actor_ptr to;
    message_id mid;
    message msg;
  };

  using hrc = std::chrono::high_resolution_clock;

  std::multimap<hrc::time_point, delayed_msg> delayed_messages;

  template <class T = resumable>
  T& next_job() {
    if (jobs.empty())
      CAF_RAISE_ERROR("jobs.empty())");
    return dynamic_cast<T&>(*jobs.front());
  }

  /// Tries to execute a single event.
  bool run_once();

  /// Executes events until the job queue is empty and no pending timeouts are
  /// left. Returns the number of processed events.
  size_t run();

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

