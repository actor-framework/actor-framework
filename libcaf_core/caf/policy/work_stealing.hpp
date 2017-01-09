/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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
#include "caf/actor_system_config.hpp"

#include "caf/policy/unprofiled.hpp"

#include "caf/detail/double_ended_queue.hpp"

namespace caf {
namespace policy {

/// Implements scheduling of actors via work stealing.
/// @extends scheduler_policy
class work_stealing : public unprofiled {
public:
  ~work_stealing() override;

  // A thread-safe queue implementation.
  using queue_type = detail::double_ended_queue<resumable>;

  using usec = std::chrono::microseconds;

  // configuration for aggressive/moderate/relaxed poll strategies.
  struct poll_strategy {
    size_t attempts;
    size_t step_size;
    size_t steal_interval;
    usec sleep_duration;
  };

  // The coordinator has only a counter for round-robin enqueue to its workers.
  template <class Worker>
  struct coordinator_data {
    explicit coordinator_data(scheduler::abstract_coordinator*)
        : next_worker(0) {
      // nop
    }

    std::atomic<size_t> next_worker;
    std::vector<std::unique_ptr<Worker>> workers;
  };

  static std::vector<poll_strategy>
  get_poll_strategies(scheduler::abstract_coordinator* p) {
    auto& x = p->system().config();
    std::vector<poll_strategy> res {
      {x.work_stealing_aggressive_poll_attempts, 1,
       x.work_stealing_aggressive_steal_interval, 
       usec{0}},
      {x.work_stealing_moderate_poll_attempts, 1,
       x.work_stealing_moderate_steal_interval,
       usec{x.work_stealing_moderate_sleep_duration_us}},
      {1, 0, x.work_stealing_relaxed_steal_interval, 
       usec {p->system().config().work_stealing_relaxed_sleep_duration_us}}
    };
    return res;
  }

  // Holds job job queue of a worker and a random number generator.
  template <class Worker>
  struct worker_data {
    explicit worker_data(scheduler::abstract_coordinator* p)
        : rengine(std::random_device{}())
          // no need to worry about wrap-around; if `p->num_workers() < 2`,
          // `uniform` will not be used anyway
        , uniform(0, p->num_workers() - 2)
        ,  strategies(get_poll_strategies(p)) {
      // nop
    }

    // This queue is exposed to other workers that may attempt to steal jobs
    // from it and the central scheduling unit can push new jobs to the queue.
    queue_type queue;
    // needed to generate pseudo random numbers
    std::default_random_engine rengine;
    std::uniform_int_distribution<size_t> uniform;
    std::vector<poll_strategy> strategies;
  };

  // Create x workers.
  template <class Coordinator, class Worker>
  void create_workers(Coordinator* self, size_t num_workers,
                                         size_t throughput) {
    for (size_t i = 0; i < num_workers; ++i) {
      std::unique_ptr<Worker> worker(new Worker(i, self, throughput));
      d(self).workers.emplace_back(std::move(worker));
    }    
  }

  /// Initalize worker thread.
  template <class Worker>
  void init_worker_thread(Worker* self) {
    self->set_all_workers_are_neighbors(true);
  }

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
    // in this case the worker id is similar to the worker index
    if (victim == self->id())
      victim = p->num_workers() - 1;
    // steal oldest element from the victim's queue
    return d(p->worker_by_idx(victim)).queue.take_tail();
  }

  template <class Coordinator>
  void central_enqueue(Coordinator* self, resumable* job) {
    auto w = self->worker_by_idx(d(self).next_worker++ % self->num_workers());
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
    auto& strategies = d(self).strategies;
    resumable* job = nullptr;
    for (auto& strat : strategies) {
      for (size_t i = 0; i < strat.attempts; i += strat.step_size) {
        job = d(self).queue.take_head();
        if (job)
          return job;
        // try to steal every X poll attempts
        if ((i % strat.steal_interval) == 0) {
          job = try_steal(self);
          if (job)
            return job;
        }
        if (strat.sleep_duration.count() > 0)
          std::this_thread::sleep_for(strat.sleep_duration);
      }
    }
    // unreachable, because the last strategy loops
    // until a job has been dequeued
    return nullptr;
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
