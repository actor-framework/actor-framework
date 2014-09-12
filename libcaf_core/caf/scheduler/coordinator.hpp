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

#ifndef CAF_SCHEDULER_COORDINATOR_HPP
#define CAF_SCHEDULER_COORDINATOR_HPP

#include <thread>
#include <limits>

#include "caf/scheduler/worker.hpp"
#include "caf/scheduler/abstract_coordinator.hpp"

namespace caf {
namespace scheduler {

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
    stop_actors();
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
} // namespace caf

#endif // CAF_SCHEDULER_COORDINATOR_HPP
