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

#ifndef CAF_SCHEDULER_WORKER_HPP
#define CAF_SCHEDULER_WORKER_HPP

#include <cstddef>

#include "caf/execution_unit.hpp"

#include "caf/detail/logging.hpp"
#include "caf/detail/double_ended_queue.hpp"

namespace caf {
namespace scheduler {

template <class Policy>
class coordinator;

/// Policy-based implementation of the abstract worker base class.
template <class Policy>
class worker : public execution_unit {
public:
  using job_ptr = resumable*;
  using coordinator_ptr = coordinator<Policy>*;
  using policy_data = typename Policy::worker_data;

  worker(size_t worker_id, coordinator_ptr worker_parent, size_t throughput)
      : max_throughput_(throughput),
        id_(worker_id),
        parent_(worker_parent) {
    // nop
  }

  void start() {
    CAF_ASSERT(this_thread_.get_id() == std::thread::id{});
    auto this_worker = this;
    this_thread_ = std::thread{[this_worker] {
      CAF_LOGF_TRACE("id = " << this_worker->id());
      this_worker->run();
    }};
  }

  worker(const worker&) = delete;
  worker& operator=(const worker&) = delete;

  /// Enqueues a new job to the worker's queue from an external
  /// source, i.e., from any other thread.
  void external_enqueue(job_ptr job) {
    CAF_ASSERT(job != nullptr);
    CAF_LOG_TRACE("id = " << id() << " actor id " << id_of(job));
    policy_.external_enqueue(this, job);
  }

  /// Enqueues a new job to the worker's queue from an internal
  /// source, i.e., a job that is currently executed by this worker.
  /// @warning Must not be called from other threads.
  void exec_later(job_ptr job) override {
    CAF_ASSERT(job != nullptr);
    CAF_LOG_TRACE("id = " << id() << " actor id " << id_of(job));
    policy_.internal_enqueue(this, job);
  }

  coordinator_ptr parent() {
    return parent_;
  }

  size_t id() const {
    return id_;
  }

  std::thread& get_thread() {
    return this_thread_;
  }

  void detach_all() {
    CAF_LOG_TRACE("");
    policy_.foreach_resumable(this, [](resumable* job) {
      job->detach_from_scheduler();
    });
  }

  actor_id id_of(resumable* ptr) {
    abstract_actor* dptr = ptr ? dynamic_cast<abstract_actor*>(ptr)
                               : nullptr;
    return dptr ? dptr->id() : 0;
  }

  policy_data& data() {
    return data_;
  }

  size_t max_throughput() {
    return max_throughput_;
  }

private:
  void run() {
    CAF_LOG_TRACE("worker with ID " << id_);
    // scheduling loop
    for (;;) {
      auto job = policy_.dequeue(this);
      CAF_ASSERT(job != nullptr);
      CAF_LOG_DEBUG("resume actor " << id_of(job));
      CAF_PUSH_AID_FROM_PTR(dynamic_cast<abstract_actor*>(job));
      policy_.before_resume(this, job);
      switch (job->resume(this, max_throughput_)) {
        case resumable::resume_later: {
          policy_.after_resume(this, job);
          policy_.resume_job_later(this, job);
          break;
        }
        case resumable::done: {
          policy_.after_resume(this, job);
          policy_.after_completion(this, job);
          job->detach_from_scheduler();
          break;
        }
        case resumable::awaiting_message: {
          // resumable will be enqueued again later
          policy_.after_resume(this, job);
          break;
        }
        case resumable::shutdown_execution_unit: {
          policy_.after_resume(this, job);
          policy_.after_completion(this, job);
          policy_.before_shutdown(this);
          return;
        }
      }
    }
  }
  // number of messages each actor is allowed to consume per resume
  size_t max_throughput_;
  // the worker's thread
  std::thread this_thread_;
  // the worker's ID received from scheduler
  size_t id_;
  // pointer to central coordinator
  coordinator_ptr parent_;
  // policy-specific data
  policy_data data_;
  // instance of our policy object
  Policy policy_;
};

} // namespace scheduler
} // namespace caf

#endif // CAF_SCHEDULER_WORKER_HPP
