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

#ifndef CAF_SCHEDULER_HPP
#define CAF_SCHEDULER_HPP

#include <chrono>
#include <memory>
#include <thread>
#include <cstdint>
#include <functional>
#include <type_traits>

#include "caf/fwd.hpp"
#include "caf/atom.hpp"
#include "caf/actor.hpp"
#include "caf/channel.hpp"
#include "caf/message.hpp"
#include "caf/duration.hpp"
#include "caf/resumable.hpp"
#include "caf/attachable.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/spawn_options.hpp"
#include "caf/execution_unit.hpp"

#include "caf/detail/logging.hpp"
#include "caf/detail/producer_consumer_list.hpp"

namespace caf {
namespace scheduler {

class abstract_coordinator;

/**
 * Base class for work-stealing workers.
 */
class abstract_worker : public execution_unit {

  friend class abstract_coordinator;

 public:

  /**
   * Attempt to steal an element from this worker.
   */
  virtual resumable* try_steal() = 0;

  /**
   * Enqueues a new job to the worker's queue from an external
   * source, i.e., from any other thread.
   */
  virtual void external_enqueue(resumable*) = 0;

  /**
   * Starts the thread of this worker.
   */
  virtual void start(size_t id, abstract_coordinator* parent) = 0;

};

/**
 * A coordinator creates the workers, manages delayed sends and
 * the central printer instance for {@link aout}. It also forwards
 * sends from detached workers or non-actor threads to randomly
 * chosen workers.
 */
class abstract_coordinator {
 public:
  friend class detail::singletons;

  explicit abstract_coordinator(size_t num_worker_threads);

  virtual ~abstract_coordinator();

  /**
   * Returns a handle to the central printing actor.
   */
  actor printer() const;

  /**
   * Puts `what` into the queue of a randomly chosen worker.
   */
  void enqueue(resumable* what);

  template <class Duration, class... Data>
  void delayed_send(Duration rel_time, actor_addr from, channel to,
            message_id mid, message data) {
    m_timer->enqueue(invalid_actor_addr, message_id::invalid,
             make_message(atom("_Send"), duration{rel_time},
                    std::move(from), std::move(to), mid,
                    std::move(data)),
             nullptr);
  }

  inline size_t num_workers() const {
    return m_num_workers;
  }

  virtual abstract_worker* worker_by_id(size_t id) = 0;

 protected:

  abstract_coordinator();

  virtual void initialize();

  virtual void stop();

 private:
  // Creates a default instance.
  static abstract_coordinator* create_singleton();

  inline void dispose() {
    delete this;
  }

  intrusive_ptr<blocking_actor> m_timer;
  scoped_actor m_printer;

  // ID of the worker receiving the next enqueue
  std::atomic<size_t> m_next_worker;

  size_t m_num_workers;

  std::thread m_timer_thread;
  std::thread m_printer_thread;
};

/**
 * Policy-based implementation of the abstract worker base class.
 */
template <class StealPolicy, class JobQueuePolicy>
class worker : public abstract_worker {
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
    m_queue_policy = std::move(other.m_queue_policy);
    m_steal_policy = std::move(other.m_steal_policy);
    return *this;
  }

  using job_ptr = resumable*;

  using job_queue = detail::producer_consumer_list<resumable>;

  /**
   * Attempt to steal an element from the exposed job queue.
   */
  job_ptr try_steal() override {
    auto result = m_queue_policy.try_external_dequeue(this);
    CAF_LOG_DEBUG_IF(result, "stole actor with id " << id_of(result));
    return result;
  }

  /**
   * Enqueues a new job to the worker's queue from an external
   * source, i.e., from any other thread.
   */
  void external_enqueue(job_ptr job) override {
    CAF_REQUIRE(job != nullptr);
    CAF_LOG_TRACE("id = " << id() << " actor id " << id_of(job));
    m_queue_policy.external_enqueue(this, job);
  }

  /**
   * Enqueues a new job to the worker's queue from an internal
   * source, i.e., a job that is currently executed by this worker.
   * @warning Must not be called from other threads.
   */
  void exec_later(job_ptr job) override {
    CAF_REQUIRE(job != nullptr);
    CAF_LOG_TRACE("id = " << id() << " actor id " << id_of(job));
    m_queue_policy.internal_enqueue(this, job);
  }

  // go on a raid in quest for a shiny new job
  job_ptr raid() {
    auto result = m_steal_policy.raid(this);
    CAF_LOG_DEBUG_IF(result, "got actor with id " << id_of(result));
    return result;
  }

  inline abstract_coordinator* parent() {
    return m_parent;
  }

  inline size_t id() const {
    return m_id;
  }

  inline std::thread& get_thread() {
    return m_this_thread;
  }

  void detach_all() {
    CAF_LOG_TRACE("");
    m_queue_policy.consume_all(this, [](resumable* job) {
      job->detach_from_scheduler();
    });
  }

  void start(size_t id, abstract_coordinator* parent) override {
    m_id = id;
    m_parent = parent;
    auto this_worker = this;
    m_this_thread = std::thread{[this_worker] {
      CAF_LOGC_TRACE("caf::scheduler::worker",
              "start$lambda",
              "id = " << this_worker->id());
      this_worker->run();
    }};
  }

  actor_id id_of(resumable* ptr) {
    abstract_actor* aptr = ptr ? dynamic_cast<abstract_actor*>(ptr)
                               : nullptr;
    return aptr ? aptr->id() : 0;
  }

 private:
  void run() {
    CAF_LOG_TRACE("worker with ID " << m_id);
    // scheduling loop
    for (;;) {
      auto job = m_queue_policy.internal_dequeue(this);
      CAF_REQUIRE(job != nullptr);
      CAF_LOG_DEBUG("resume actor " << id_of(job));
      CAF_PUSH_AID_FROM_PTR(dynamic_cast<abstract_actor*>(job));
      switch (job->resume(this)) {
        case resumable::done: {
          job->detach_from_scheduler();
          break;
        }
        case resumable::resume_later: {
          break;
        }
        case resumable::shutdown_execution_unit: {
          m_queue_policy.clear_internal_queue(this);
          return;
        }
      }
      m_queue_policy.assert_stealable(this);
    }
  }

  // the worker's thread
  std::thread m_this_thread;
  // the worker's ID received from scheduler
  size_t m_id;
  // pointer to central coordinator
  abstract_coordinator* m_parent;
  // policy managing queues
  JobQueuePolicy m_queue_policy;
  // policy managing steal operations
  StealPolicy m_steal_policy;
};

/**
 * Policy-based implementation of the abstract coordinator base class.
 */
template <class StealPolicy, class JobQueuePolicy>
class coordinator : public abstract_coordinator {
 public:
  using super = abstract_coordinator;

  coordinator(size_t nw = std::thread::hardware_concurrency()) : super(nw) {
    // nop
  }

  using worker_type = worker<StealPolicy, JobQueuePolicy>;

  abstract_worker* worker_by_id(size_t id) override {
    return &m_workers[id];
  }

 protected:
  void initialize() override {
    super::initialize();
    // create & start workers
    m_workers.resize(num_workers());
    for (size_t i = 0; i < num_workers(); ++i) {
      m_workers[i].start(i, this);
    }
  }

  void stop() override {
    super::stop();
    // wait until all actors are done
    for (auto& w : m_workers) w.get_thread().join();
    // clear all queues
    for (auto& w : m_workers) w.detach_all();
  }

 private:
  // usually of size std::thread::hardware_concurrency()
  std::vector<worker_type> m_workers;
};

} // namespace scheduler

/**
 * Sets a user-defined scheduler.
 * @note This function must be used before actor is spawned. Dynamically
 *       changing the scheduler at runtime is not supported.
 * @throws std::logic_error if a scheduler is already defined
 */
void set_scheduler(scheduler::abstract_coordinator* ptr);

/**
 * Sets a user-defined scheduler using given policies. The scheduler
 * is instantiated with `nw` number of workers.
 * @note This function must be used before actor is spawned. Dynamically
 *       changing the scheduler at runtime is not supported.
 * @throws std::logic_error if a scheduler is already defined
 */
template <class StealPolicy, class JobQueuePolicy>
void set_scheduler(size_t nw = std::thread::hardware_concurrency()) {
  set_scheduler(new scheduler::coordinator<StealPolicy, JobQueuePolicy>(nw));
}

} // namespace caf

#endif // CAF_SCHEDULER_HPP
