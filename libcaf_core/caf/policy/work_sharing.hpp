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

#include <list>
#include <mutex>
#include <cstddef>
#include <condition_variable>

#include "caf/resumable.hpp"
#include "caf/policy/unprofiled.hpp"

namespace caf {
namespace policy {

/// Implements scheduling of actors via work sharing (central job queue).
/// @extends scheduler_policy
class work_sharing : public unprofiled {
public:
  // A thread-safe queue implementation.
  using queue_type = std::list<resumable*>;

  ~work_sharing() override;

  struct coordinator_data {
    inline explicit coordinator_data(scheduler::abstract_coordinator*) {
      // nop
    }

    queue_type queue;
    std::mutex lock;
    std::condition_variable cv;
  };

  struct worker_data {
    inline explicit worker_data(scheduler::abstract_coordinator*) {
      // nop
    }
  };

  template <class Coordinator>
  void enqueue(Coordinator* self, resumable* job) {
    queue_type l;
    l.push_back(job);
    std::unique_lock<std::mutex> guard(d(self).lock);
    d(self).queue.splice(d(self).queue.end(), l);
    d(self).cv.notify_one();
  }

  template <class Coordinator>
  void central_enqueue(Coordinator* self, resumable* job) {
    enqueue(self, job);
  }

  template <class Worker>
  void external_enqueue(Worker* self, resumable* job) {
    enqueue(self->parent(), job);
  }

  template <class Worker>
  void internal_enqueue(Worker* self, resumable* job) {
    enqueue(self->parent(), job);
  }

  template <class Worker>
  void resume_job_later(Worker* self, resumable* job) {
    // job has voluntarily released the CPU to let others run instead
    // this means we are going to put this job to the very end of our queue
    enqueue(self->parent(), job);
  }

  template <class Worker>
  resumable* dequeue(Worker* self) {
    auto& parent_data = d(self->parent());
    std::unique_lock<std::mutex> guard(parent_data.lock);
    parent_data.cv.wait(guard, [&] { return !parent_data.queue.empty(); });
    resumable* job = parent_data.queue.front();
    parent_data.queue.pop_front();
    return job;
  }

  template <class Worker, class UnaryFunction>
  void foreach_resumable(Worker*, UnaryFunction) {
    // nop
  }

  template <class Coordinator, class UnaryFunction>
  void foreach_central_resumable(Coordinator* self, UnaryFunction f) {
    auto& queue = d(self).queue;
    auto next = [&]() -> resumable* {
      if (queue.empty()) {
        return nullptr;
      }
      auto front = queue.front();
      queue.pop_front();
      return front;
    };
    std::unique_lock<std::mutex> guard(d(self).lock);
    for (auto job = next(); job != nullptr; job = next()) {
      f(job);
    }
  }
};

} // namespace policy
} // namespace caf

