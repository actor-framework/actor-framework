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

#ifndef CAF_POLICY_WORK_SHARING_HPP
#define CAF_POLICY_WORK_SHARING_HPP

#include <list>
#include <mutex>
#include <cstddef>
#include <condition_variable>

#include "caf/resumable.hpp"

namespace caf {
namespace policy {

/// @extends scheduler_policy
class work_sharing {
public:
  // A thead-safe queue implementation.
  using queue_type = std::list<resumable*>;

  struct coordinator_data {
    queue_type queue;
    std::mutex lock;
    std::condition_variable cv;
    inline coordinator_data() {
      // nop
    }
  };

  struct worker_data {
    inline worker_data() {
      // nop
    }
  };

  // Convenience function to access the data field.
  template <class WorkerOrCoordinator>
  static auto d(WorkerOrCoordinator* self) -> decltype(self->data()) {
    return self->data();
  }

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
    auto& parent_data = d(self->parent());
    queue_type l;
    l.push_back(job);
    std::unique_lock<std::mutex> guard(parent_data.lock);
    parent_data.queue.splice(parent_data.queue.begin(), l);
    parent_data.cv.notify_one();
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
    while (parent_data.queue.empty()) {
      parent_data.cv.wait(guard);
    }
    resumable* job = parent_data.queue.front();
    parent_data.queue.pop_front();
    return job;
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
    auto next = [&]() -> resumable* {
      if (d(self->parent()).queue.empty()) {
        return nullptr;
      }
      auto front = d(self->parent()).queue.front();
      d(self->parent()).queue.pop_front();
      return front;
    };
    std::unique_lock<std::mutex> guard(d(self->parent()).lock);
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

#endif // CAF_POLICY_WORK_SHARING_HPP
