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

#ifndef CAF_SCHEDULER_WORKER_HPP
#define CAF_SCHEDULER_WORKER_HPP

#include <cstddef>

#include "caf/execution_unit.hpp"

#include "caf/detail/logging.hpp"
#include "caf/detail/producer_consumer_list.hpp"

namespace caf {
namespace scheduler {

template <class Policy>
class coordinator;

/**
 * Policy-based implementation of the abstract worker base class.
 */
template <class Policy>
class worker : public execution_unit {
 public:
  worker(const worker&) = delete;
  worker& operator=(const worker&) = delete;

  worker() = default;

  worker(worker&& other) {
    *this = std::move(other); // delegate to move assignment operator
  }

  worker& operator=(worker&& other) {
    // cannot be moved once m_this_thread is up and running
    auto running = [](std::thread& t) {
      return t.get_id() != std::thread::id{};
    };
    if (running(m_this_thread) || running(other.m_this_thread)) {
      throw std::runtime_error("running workers cannot be moved");
    }
    m_policy = std::move(other.m_policy);
    m_policy = std::move(other.m_policy);
    return *this;
  }

  using coordinator_type = coordinator<Policy>;

  using job_ptr = resumable*;

  using job_queue = detail::producer_consumer_list<resumable>;

  using policy_data = typename Policy::worker_data;

  /**
   * Enqueues a new job to the worker's queue from an external
   * source, i.e., from any other thread.
   */
  void external_enqueue(job_ptr job) {
    CAF_REQUIRE(job != nullptr);
    CAF_LOG_TRACE("id = " << id() << " actor id " << id_of(job));
    m_policy.external_enqueue(this, job);
  }

  /**
   * Enqueues a new job to the worker's queue from an internal
   * source, i.e., a job that is currently executed by this worker.
   * @warning Must not be called from other threads.
   */
  void exec_later(job_ptr job) override {
    CAF_REQUIRE(job != nullptr);
    CAF_LOG_TRACE("id = " << id() << " actor id " << id_of(job));
    m_policy.internal_enqueue(this, job);
  }

  // go on a raid in quest for a shiny new job
  job_ptr raid() {
    auto result = m_policy.raid(this);
    CAF_LOG_DEBUG_IF(result, "got actor with id " << id_of(result));
    return result;
  }

  coordinator_type* parent() {
    return m_parent;
  }

  size_t id() const {
    return m_id;
  }

  std::thread& get_thread() {
    return m_this_thread;
  }

  void detach_all() {
    CAF_LOG_TRACE("");
    m_policy.foreach_resumable(this, [](resumable* job) {
      job->detach_from_scheduler();
    });
  }

  void start(size_t id, coordinator_type* parent, size_t max_throughput) {
    m_max_throughput = max_throughput;
    m_id = id;
    m_parent = parent;
    auto this_worker = this;
    m_this_thread = std::thread{[this_worker] {
      CAF_LOGC_TRACE("caf::scheduler::worker", "start$lambda",
                     "id = " << this_worker->id());
      this_worker->run();
    }};
  }

  actor_id id_of(resumable* ptr) {
    abstract_actor* aptr = ptr ? dynamic_cast<abstract_actor*>(ptr)
                               : nullptr;
    return aptr ? aptr->id() : 0;
  }

  policy_data& data() {
    return m_data;
  }

  size_t max_throughput() {
    return m_max_throughput;
  }

 private:
  void run() {
    CAF_LOG_TRACE("worker with ID " << m_id);
    // scheduling loop
    for (;;) {
      auto job = m_policy.dequeue(this);
      CAF_REQUIRE(job != nullptr);
      CAF_LOG_DEBUG("resume actor " << id_of(job));
      CAF_PUSH_AID_FROM_PTR(dynamic_cast<abstract_actor*>(job));
      switch (job->resume(this, m_max_throughput)) {
        case resumable::resume_later: {
          m_policy.resume_job_later(this, job);
          break;
        }
        case resumable::done: {
          job->detach_from_scheduler();
          break;
        }
        case resumable::awaiting_message: {
          // resumable will be enqueued again later
          break;
        }
        case resumable::shutdown_execution_unit: {
          m_policy.before_shutdown(this);
          return;
        }
      }
      m_policy.after_resume(this);
    }
  }
  // number of messages each actor is allowed to consume per resume
  size_t m_max_throughput;
  // the worker's thread
  std::thread m_this_thread;
  // the worker's ID received from scheduler
  size_t m_id;
  // pointer to central coordinator
  coordinator_type* m_parent;
  // policy-specific data
  policy_data m_data;
  // instance of our policy object
  Policy m_policy;
};

} // namespace scheduler
} // namespace caf

#endif // CAF_SCHEDULER_WORKER_HPP
