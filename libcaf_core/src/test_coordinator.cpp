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

#include "caf/scheduler/test_coordinator.hpp"

#include <limits>

#include "caf/actor_system_config.hpp"
#include "caf/monitorable_actor.hpp"
#include "caf/raise_error.hpp"
#include "caf/resumable.hpp"

namespace caf {
namespace scheduler {

namespace {

class dummy_worker : public execution_unit {
public:
  dummy_worker(test_coordinator* parent)
      : execution_unit(&parent->system()),
        parent_(parent) {
    // nop
  }

  void exec_later(resumable* ptr) override {
    parent_->jobs.push_back(ptr);
  }

private:
  test_coordinator* parent_;
};

class dummy_printer : public monitorable_actor {
public:
  dummy_printer(actor_config& cfg) : monitorable_actor(cfg) {
    mh_.assign(
      [&](add_atom, actor_id, const std::string& str) {
        std::cout << str;
      }
    );
  }

  void enqueue(mailbox_element_ptr what, execution_unit*) override {
    mh_(what->content());
  }

private:
  message_handler mh_;
};

} // namespace <anonymous>

test_coordinator::test_coordinator(actor_system& sys) : super(sys) {
  // nop
}

bool test_coordinator::detaches_utility_actors() const {
  return false;
}

detail::test_actor_clock& test_coordinator::clock() noexcept {
  return clock_;
}

void test_coordinator::start() {
  dummy_worker worker{this};
  actor_config cfg{&worker};
  auto& sys = system();
  utility_actors_[printer_id] = make_actor<dummy_printer, actor>(
    sys.next_actor_id(), sys.node(), &sys, cfg);
}

void test_coordinator::stop() {
  run_cycle();
}

void test_coordinator::enqueue(resumable* ptr) {
  CAF_LOG_TRACE("");
  jobs.push_back(ptr);
  if (after_next_enqueue_ != nullptr) {
    std::function<void()> f;
    f.swap(after_next_enqueue_);
    f();
  }
}

bool test_coordinator::try_run_once() {
  if (jobs.empty())
    return false;
  auto job = jobs.front();
  jobs.pop_front();
  dummy_worker worker{this};
  switch (job->resume(&worker, 1)) {
    case resumable::resume_later:
      jobs.push_front(job);
      break;
    case resumable::done:
    case resumable::awaiting_message:
      intrusive_ptr_release(job);
      break;
    case resumable::shutdown_execution_unit:
      break;
  }
  return true;
}

bool test_coordinator::try_run_once_lifo() {
  if (jobs.empty())
    return false;
  if (jobs.size() >= 2)
    std::rotate(jobs.rbegin(), jobs.rbegin() + 1, jobs.rend());
  return try_run_once();
}

void test_coordinator::run_once() {
  if (jobs.empty())
    CAF_RAISE_ERROR("No job to run available.");
  try_run_once();
}

void test_coordinator::run_once_lifo() {
  if (jobs.empty())
    CAF_RAISE_ERROR("No job to run available.");
  try_run_once_lifo();
}

size_t test_coordinator::run(size_t max_count) {
  size_t res = 0;
  while (res < max_count && try_run_once())
    ++res;
  return res;
}

std::pair<size_t, size_t>
test_coordinator::run_cycle_until(bool_predicate predicate,
                                  timespan cycle_duration) {
  CAF_LOG_TRACE(CAF_ARG(cycle_duration)
                << CAF_ARG2("pending_jobs", jobs.size())
                << CAF_ARG2("pending_timeouts", clock_.schedule().size()));
  // Bookkeeping.
  size_t events = 0;
  size_t timeouts = 0;
  // Loop until no activity remains.
  while (!jobs.empty() || !clock_.schedule().empty()) {
    while (try_run_once()) {
      ++events;
      if (predicate()) {
        CAF_LOG_DEBUG("stop due to predicate;"
                      << CAF_ARG(events) << CAF_ARG(timeouts));
        return {events, timeouts};
      }
    }
    if (!clock_.schedule().empty()) {
      size_t triggered = 0;
      auto next_tout = clock_.schedule().begin()->first;
      if (next_tout <= clock_.now()) {
        // Trigger expired timeout first without advancing time.
        triggered = clock_.trigger_expired_timeouts();
      } else {
        // Compute how much cycles we need to trigger at least one timeout.
        auto diff = next_tout - clock_.current_time;
        auto num_cycles = diff.count() / cycle_duration.count();
        auto advancement = num_cycles * cycle_duration;
        // `num_cyles` can have one to few cycles since we's always rounding down.
        if (clock_.current_time + advancement < next_tout)
          advancement += cycle_duration;
        // Advance time.
        CAF_ASSERT(advancement.count() > 0);
        triggered = clock_.advance_time(advancement);
      }
      CAF_ASSERT(triggered > 0);
      timeouts += triggered;
    }
  }
  CAF_LOG_DEBUG("no activity left;" << CAF_ARG(events) << CAF_ARG(timeouts));
  return {events, timeouts};
}

std::pair<size_t, size_t> test_coordinator::run_cycle(timespan cycle_duration) {
  return run_cycle_until([] { return false; }, cycle_duration);
}

void test_coordinator::inline_next_enqueue() {
  after_next_enqueue([=] { run_once_lifo(); });
}

void test_coordinator::inline_all_enqueues() {
  after_next_enqueue([=] { inline_all_enqueues_helper(); });
}

void test_coordinator::inline_all_enqueues_helper() {
  run_once_lifo();
  after_next_enqueue([=] { inline_all_enqueues_helper(); });
}

std::pair<size_t, size_t>
test_coordinator::run_dispatch_loop(timespan cycle_duration) {
  return run_cycle(cycle_duration);
}

} // namespace caf
} // namespace scheduler

