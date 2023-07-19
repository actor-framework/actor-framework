// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/policy/unprofiled.hpp"
#include "caf/resumable.hpp"

#include <condition_variable>
#include <cstddef>
#include <list>
#include <mutex>

namespace caf::policy {

/// Implements scheduling of actors via work sharing (central job queue).
/// @extends scheduler_policy
class CAF_CORE_EXPORT work_sharing : public unprofiled {
public:
  // A thread-safe queue implementation.
  using queue_type = std::list<resumable*>;

  ~work_sharing() override;

  struct coordinator_data {
    explicit coordinator_data(scheduler::abstract_coordinator*) {
      // nop
    }

    queue_type queue;
    std::mutex lock;
    std::condition_variable cv;
  };

  struct worker_data {
    explicit worker_data(scheduler::abstract_coordinator*) {
      // nop
    }
  };

  template <class Coordinator>
  bool enqueue(Coordinator* self, resumable* job) {
    queue_type l;
    l.push_back(job);
    std::unique_lock<std::mutex> guard(d(self).lock);
    d(self).queue.splice(d(self).queue.end(), l);
    d(self).cv.notify_one();
    return true;
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

} // namespace caf::policy
