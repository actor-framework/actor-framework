/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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

#ifndef CAF_POLICY_WORK_STEALING_HPP
#define CAF_POLICY_WORK_STEALING_HPP

#include <deque>
#include <chrono>
#include <thread>
#include <random>
#include <cstddef>

#include "caf/resumable.hpp"

#include "caf/detail/double_ended_queue.hpp"

namespace caf {
namespace policy {

/// Implements scheduling of actors via work stealing. This implementation uses
/// two queues: a synchronized queue accessible by other threads and an internal
/// queue. Access to the synchronized queue is minimized.
/// The reasoning behind this design decision is that it
/// has been shown that stealing actually is very rare for most workloads [1].
/// Hence, implementations should focus on the performance in
/// the non-stealing case. For this reason, each worker has an exposed
/// job queue that can be accessed by the central scheduler instance as
/// well as other workers, but it also has a private job list it is
/// currently working on. To account for the load balancing aspect, each
/// worker makes sure that at least one job is left in its exposed queue
/// to allow other workers to steal it.
///
/// [1] http://dl.acm.org/citation.cfm?doid=2398857.2384639
///
/// @extends scheduler_policy
class work_stealing {
public:
  // A thead-safe queue implementation.
  using queue_type = detail::double_ended_queue<resumable>;

  // The coordinator has only a counter for round-robin enqueue to its workers.
  struct coordinator_data {
    std::atomic<size_t> next_worker;
    inline coordinator_data() : next_worker(0) {
      // nop
    }
  };

  // Holds job job queue of a worker and a random number generator.
  struct worker_data {
    // This queue is exposed to other workers that may attempt to steal jobs
    // from it and the central scheduling unit can push new jobs to the queue.
    queue_type queue;
    // needed by our engine
    std::random_device rdevice;
    // needed to generate pseudo random numbers
    std::default_random_engine rengine;
    // initialize random engine
    inline worker_data() : rdevice(), rengine(rdevice()) {
      // nop
    }
  };

  // Convenience function to access the data field.
  template <class WorkerOrCoordinator>
  static auto d(WorkerOrCoordinator* self) -> decltype(self->data()) {
    return self->data();
  }

  // Goes on a raid in quest for a shiny new job.
  template <class Worker>
  resumable* try_steal(Worker* self) {
    auto p = self->parent();
    if (p->num_workers() < 2) {
      // you can't steal from yourself, can you?
      return nullptr;
    }
    size_t victim;
    do {
      // roll the dice to pick a victim other than ourselves
      victim = d(self).rengine() % p->num_workers();
    }
    while (victim == self->id());
    // steal oldest element from the victim's queue
    return d(p->worker_by_id(victim)).queue.take_tail();
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
    d(self).queue.append(job);
  }

  template <class Worker>
  resumable* dequeue(Worker* self) {
    // we wait for new jobs by polling our external queue: first, we
    // assume an active work load on the machine and perform aggresive
    // polling, then we relax our polling a bit and wait 50 us between
    // dequeue attempts, finally we assume pretty much nothing is going
    // on and poll every 10 ms; this strategy strives to minimize the
    // downside of "busy waiting", which still performs much better than a
    // "signalizing" implementation based on mutexes and conition variables
    struct poll_strategy {
      size_t attempts;
      size_t step_size;
      size_t steal_interval;
      std::chrono::microseconds sleep_duration;
    };
    poll_strategy strategies[3] = {
      // aggressive polling  (100x) without sleep interval
      {100, 1, 10, std::chrono::microseconds{0}},
      // moderate polling (500x) with 50 us sleep interval
      {500, 1, 5,  std::chrono::microseconds{50}},
      // relaxed polling (infinite attempts) with 10 ms sleep interval
      {101, 0, 1,  std::chrono::microseconds{10000}}
    };
    resumable* job = nullptr;
    for (auto& strat : strategies) {
      for (size_t i = 0; i < strat.attempts; i += strat.step_size) {
        job = d(self).queue.take_head();
        if (job) {
          return job;
        }
        // try to steal every X poll attempts
        if ((i % strat.steal_interval) == 0) {
          job = try_steal(self);
          if (job) {
            return job;
          }
        }
        std::this_thread::sleep_for(strat.sleep_duration);
      }
    }
    // unreachable, because the last strategy loops
    // until a job has been dequeued
    return nullptr;
  }

  template <class Worker>
  void before_shutdown(Worker*) {
    // nop
  }

  template <class Worker>
  void before_resume(Worker*, resumable*) {
    // nop
  }

  template <class Worker>
  void after_resume(Worker*, resumable*) {
    // nop
  }

  template <class Worker>
  void after_completion(Worker*, resumable*) {
    // nop
  }

  template <class Worker, class UnaryFunction>
  void foreach_resumable(Worker* self, UnaryFunction f) {
    auto next = [&] { return d(self).queue.take_head(); };
    for (auto job = next(); job != nullptr; job = next()) {
      f(job);
    }
  }

  template <class Coordinator, class UnaryFunction>
  void foreach_central_resumable(Coordinator*, UnaryFunction) {
    // nop
  }
};

} // namespace policy
} // namespace caf

#endif // CAF_POLICY_WORK_STEALING_HPP
