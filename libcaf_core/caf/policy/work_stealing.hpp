// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/actor_system_config.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/double_ended_queue.hpp"
#include "caf/policy/unprofiled.hpp"
#include "caf/resumable.hpp"
#include "caf/timespan.hpp"

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <random>
#include <thread>

namespace caf::policy {

/// Implements scheduling of actors via work stealing.
/// @extends scheduler_policy
class CAF_CORE_EXPORT work_stealing : public unprofiled {
public:
  ~work_stealing() override;

  // A thread-safe queue implementation.
  using queue_type = detail::double_ended_queue<resumable>;

  // configuration for aggressive/moderate/relaxed poll strategies.
  struct poll_strategy {
    size_t attempts;
    size_t step_size;
    size_t steal_interval;
    timespan sleep_duration;
  };

  // The coordinator has only a counter for round-robin enqueue to its workers.
  struct coordinator_data {
    explicit coordinator_data(scheduler::abstract_coordinator*)
      : next_worker(0) {
      // nop
    }

    std::atomic<size_t> next_worker;
  };

  // Holds job job queue of a worker and a random number generator.
  struct worker_data {
    explicit worker_data(scheduler::abstract_coordinator* p);
    worker_data(const worker_data& other);

    // This queue is exposed to other workers that may attempt to steal jobs
    // from it and the central scheduling unit can push new jobs to the queue.
    queue_type queue;
    // needed to generate pseudo random numbers
    std::default_random_engine rengine;
    std::uniform_int_distribution<size_t> uniform;
    std::array<poll_strategy, 3> strategies;
  };

  // Goes on a raid in quest for a shiny new job.
  template <class Worker>
  resumable* try_steal(Worker* self) {
    auto p = self->parent();
    if (p->num_workers() < 2) {
      // you can't steal from yourself, can you?
      return nullptr;
    }
    // roll the dice to pick a victim other than ourselves
    auto victim = d(self).uniform(d(self).rengine);
    if (victim == self->id())
      victim = p->num_workers() - 1;
    // steal oldest element from the victim's queue
    return d(p->worker_by_id(victim)).queue.try_take_tail();
  }

  template <class Coordinator>
  void central_enqueue(Coordinator* self, resumable* job) {
    auto w = self->worker_by_id(d(self).next_worker++ % self->num_workers());
    w->external_enqueue(job);
  }

  template <class Worker>
  void external_enqueue(Worker* self, resumable* job) {
    d(self).queue.append(job);
  }

  template <class Worker>
  void internal_enqueue(Worker* self, resumable* job) {
    d(self).queue.prepend(job);
  }

  template <class Worker>
  void resume_job_later(Worker* self, resumable* job) {
    // job has voluntarily released the CPU to let others run instead
    // this means we are going to put this job to the very end of our queue
    d(self).queue.unsafe_append(job);
  }

  template <class Worker>
  resumable* dequeue(Worker* self) {
    // we wait for new jobs by polling our external queue: first, we
    // assume an active work load on the machine and perform aggressive
    // polling, then we relax our polling a bit and wait 50 us between
    // dequeue attempts
    auto& strategies = d(self).strategies;
    auto* job = d(self).queue.try_take_head();
    if (job)
      return job;
    for (size_t k = 0; k < 2; ++k) { // iterate over the first two strategies
      for (size_t i = 0; i < strategies[k].attempts;
           i += strategies[k].step_size) {
        // try to steal every X poll attempts
        if ((i % strategies[k].steal_interval) == 0) {
          job = try_steal(self);
          if (job)
            return job;
        }
        // wait for some work to appear
        job = d(self).queue.try_take_head(strategies[k].sleep_duration);
        if (job)
          return job;
      }
    }
    // we assume pretty much nothing is going on so we can relax polling
    // and falling to sleep on a condition variable whose timeout is the one
    // of the relaxed polling strategy
    auto& relaxed = strategies[2];
    do {
      job = d(self).queue.try_take_head(relaxed.sleep_duration);
    } while (job == nullptr);
    return job;
  }

  template <class Worker, class UnaryFunction>
  void foreach_resumable(Worker* self, UnaryFunction f) {
    auto next = [&] { return d(self).queue.try_take_head(); };
    for (auto job = next(); job != nullptr; job = next()) {
      f(job);
    }
  }

  template <class Coordinator, class UnaryFunction>
  void foreach_central_resumable(Coordinator*, UnaryFunction) {
    // nop
  }
};

} // namespace caf::policy
