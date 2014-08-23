/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENCE_ALTERNATIVE.       *
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

#include "caf/detail/producer_consumer_list.hpp"

namespace caf {
namespace policy {

/**
 * Implements scheduling of actors via work stealing. This implementation uses
 * two queues: a synchronized queue accessible by other threads and an internal
 * queue. Access to the synchronized queue is minimized.
 * The reasoning behind this design decision is that it
 * has been shown that stealing actually is very rare for most workloads [1].
 * Hence, implementations should focus on the performance in
 * the non-stealing case. For this reason, each worker has an exposed
 * job queue that can be accessed by the central scheduler instance as
 * well as other workers, but it also has a private job list it is
 * currently working on. To account for the load balancing aspect, each
 * worker makes sure that at least one job is left in its exposed queue
 * to allow other workers to steal it.
 *
 * [1] http://dl.acm.org/citation.cfm?doid=2398857.2384639
 *
 * @extends scheduler_policy
 */
class work_stealing {
 public:
  // A thead-safe queue implementation.
  using sync_queue = detail::producer_consumer_list<resumable>;

  // A queue implementation supporting fast push and pop
  // operations on both ends of the queue.
  using priv_queue = std::deque<resumable*>;

  // The coordinator has no data since our scheduling is decentralized.
  struct coordinator_data {
    size_t next_worker;
    inline coordinator_data() : next_worker(0) {
      // nop
    }
  };

  // Holds the job queues of a worker.
  struct worker_data {
    // This queue is exposed to other workers that may attempt to steal jobs
    // from it and the central scheduling unit can push new jobs to the queue.
    sync_queue exposed_queue;
    // Internal job queue of a worker (not exposed to others).
    priv_queue private_queue;
    // needed by our engine
    std::random_device rdevice;
    // needed to generate pseudo random numbers
    std::default_random_engine rengine;
    inline worker_data() : rdevice(), rengine(rdevice()) {
      // nop
    }
  };

  // convenience function to access the data field
  template <class WorkerOrCoordinator>
  auto d(WorkerOrCoordinator* self) -> decltype(self->data()) {
    return self->data();
  }

  // go on a raid in quest for a shiny new job
  template <class Worker>
  resumable* try_steal(Worker* self) {
    auto p = self->parent();
    size_t victim;
    do {
      // roll the dice to pick a victim other than ourselves
      victim = d(self).rengine() % p->num_workers();
    }
    while (victim == self->id());
    return d(p->worker_by_id(victim)).exposed_queue.try_pop();
  }

  template <class Coordinator>
  void central_enqueue(Coordinator* self, resumable* job) {
    auto w = self->worker_by_id(d(self).next_worker++ % self->num_workers());
    w->external_enqueue(job);
  }

  template <class Worker>
  void external_enqueue(Worker* self, resumable* job) {
    d(self).exposed_queue.push_back(job);
  }

  template <class Worker>
  void internal_enqueue(Worker* self, resumable* job) {
    d(self).private_queue.push_back(job);
    // give others the opportunity to steal from us
    after_resume(self);
  }

  template <class Worker>
  void resume_job_later(Worker* self, resumable* job) {
    // job has voluntarily released the CPU to let others run instead
    // this means we are going to put this job to the very end of our queue
    // by moving everything from the exposed to private queue first and
    // then enqueue job to the exposed queue
    auto next = [&] {
      return d(self).exposed_queue.try_pop();
    };
    for (auto ptr = next(); ptr != nullptr; ptr = next()) {
      d(self).private_queue.push_front(ptr);
    }
    d(self).exposed_queue.push_back(job);
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
    constexpr poll_strategy strategies[3] = {
      // aggressive polling  (100x) without sleep interval
      {100, 1, 10, std::chrono::microseconds{0}},
      // moderate polling (500x) with 50 us sleep interval
      {500, 1, 5,  std::chrono::microseconds{50}},
      // relaxed polling (infinite attempts) with 10 ms sleep interval
      {101, 0, 1,  std::chrono::microseconds{10000}}
    };
    resumable* job = nullptr;
    // local poll
    if (!d(self).private_queue.empty()) {
      job = d(self).private_queue.back();
      d(self).private_queue.pop_back();
      return job;
    }
    for (auto& strat : strategies) {
      for (size_t i = 0; i < strat.attempts; i += strat.step_size) {
        job = d(self).exposed_queue.try_pop();
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
  void before_shutdown(Worker* self) {
    // give others the opportunity to steal unfinished jobs
    for (auto ptr : d(self).private_queue) {
      d(self).exposed_queue.push_back(ptr);
    }
    d(self).private_queue.clear();
  }

  template <class Worker>
  void after_resume(Worker* self) {
    // give others the opportunity to steal from us
    if (d(self).private_queue.size() > 1 && d(self).exposed_queue.empty()) {
      d(self).exposed_queue.push_back(d(self).private_queue.front());
      d(self).private_queue.pop_front();
    }
  }

  template <class Worker, class UnaryFunction>
  void foreach_resumable(Worker* self, UnaryFunction f) {
    for (auto job : d(self).private_queue) {
      f(job);
    }
    d(self).private_queue.clear();
    auto next = [&] { return this->d(self).exposed_queue.try_pop(); };
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
