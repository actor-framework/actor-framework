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
#include <limits>
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

#include "caf/policy/work_stealing.hpp"

#include "caf/detail/logging.hpp"
#include "caf/detail/producer_consumer_list.hpp"

namespace caf {
namespace scheduler {

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
  virtual void enqueue(resumable* what) = 0;

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

  //virtual execution_unit* worker_by_id(size_t id) = 0;

 protected:

  abstract_coordinator();

  virtual void initialize();

  virtual void stop() = 0;

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

/**
 * Policy-based implementation of the abstract coordinator base class.
 */
template <class Policy>
class coordinator : public abstract_coordinator {
 public:
  using super = abstract_coordinator;

  using policy_data = typename Policy::coordinator_data;

  coordinator(size_t nw = std::thread::hardware_concurrency(),
              size_t mt = std::numeric_limits<size_t>::max())
    : super(nw),
      m_max_throughput(mt) {
    // nop
  }

  using worker_type = worker<Policy>;

  worker_type* worker_by_id(size_t id) {//override {
    return &m_workers[id];
  }

  policy_data& data() {
    return m_data;
  }

 protected:
  void initialize() override {
    super::initialize();
    // create & start workers
    m_workers.resize(num_workers());
    for (size_t i = 0; i < num_workers(); ++i) {
      m_workers[i].start(i, this, m_max_throughput);
    }
  }

  void stop() override {
    // perform cleanup code of base classe
    CAF_LOG_TRACE("");
    // shutdown workers
    class shutdown_helper : public resumable {
     public:
      void attach_to_scheduler() override {
        // nop
      }
      void detach_from_scheduler() override {
        // nop
      }
      resumable::resume_result resume(execution_unit* ptr, size_t) override {
        CAF_LOG_DEBUG("shutdown_helper::resume => shutdown worker");
        CAF_REQUIRE(ptr != nullptr);
        std::unique_lock<std::mutex> guard(mtx);
        last_worker = ptr;
        cv.notify_all();
        return resumable::shutdown_execution_unit;
      }
      shutdown_helper() : last_worker(nullptr) {
        // nop
      }
      std::mutex mtx;
      std::condition_variable cv;
      execution_unit* last_worker;
    };
    shutdown_helper sh;
    std::vector<worker_type*> alive_workers;
    auto num = num_workers();
    for (size_t i = 0; i < num; ++i) {
      alive_workers.push_back(worker_by_id(i));
    }
    CAF_LOG_DEBUG("enqueue shutdown_helper into each worker");
    while (!alive_workers.empty()) {
      alive_workers.back()->external_enqueue(&sh);
      // since jobs can be stolen, we cannot assume that we have
      // actually shut down the worker we've enqueued sh to
      { // lifetime scope of guard
        std::unique_lock<std::mutex> guard(sh.mtx);
        sh.cv.wait(guard, [&] { return sh.last_worker != nullptr; });
      }
      auto last = alive_workers.end();
      auto i = std::find(alive_workers.begin(), last, sh.last_worker);
      sh.last_worker = nullptr;
      alive_workers.erase(i);
    }
    // shutdown utility actors
    CAF_LOG_DEBUG("send exit messages to timer & printer");
    anon_send_exit(this->m_timer->address(), exit_reason::user_shutdown);
    anon_send_exit(this->m_printer->address(), exit_reason::user_shutdown);
    CAF_LOG_DEBUG("join threads of utility actors");
    // join each worker thread for good manners
    m_timer_thread.join();
    m_printer_thread.join();
    // wait until all workers are done
    for (auto& w : m_workers) {
      w.get_thread().join();
    }
    // run cleanup code for each resumable
    auto f = [](resumable* job) {
      job->detach_from_scheduler();
    };
    for (auto& w : m_workers) {
      m_policy.foreach_resumable(&w, f);
    }
    m_policy.foreach_central_resumable(this, f);
  }

  void enqueue(resumable* ptr) {
    m_policy.central_enqueue(this, ptr);
  }

 private:
  // usually of size std::thread::hardware_concurrency()
  std::vector<worker_type> m_workers;
  // policy-specific data
  policy_data m_data;
  // instance of our policy object
  Policy m_policy;
  // number of messages each actor is allowed to consume per resume
  size_t m_max_throughput;
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
 * is instantiated with `nw` number of workers and allows each actor
 * to consume up to `max_throughput` per resume (must be > 0).
 * @note This function must be used before actor is spawned. Dynamically
 *       changing the scheduler at runtime is not supported.
 * @throws std::logic_error if a scheduler is already defined
 * @throws std::invalid_argument if `max_throughput == 0`
 */
template <class Policy = policy::work_stealing>
void set_scheduler(size_t nw = std::thread::hardware_concurrency(),
                   size_t max_throughput = std::numeric_limits<size_t>::max()) {
  if (max_throughput == 0) {
    throw std::invalid_argument("max_throughput must not be 0");
  }
  set_scheduler(new scheduler::coordinator<Policy>(nw, max_throughput));
}

} // namespace caf

#endif // CAF_SCHEDULER_HPP
