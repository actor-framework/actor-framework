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

#ifndef CAF_SCHEDULER_COORDINATOR_HPP
#define CAF_SCHEDULER_COORDINATOR_HPP

#include <thread>
#include <limits>
#include <memory>
#include <condition_variable>

#include "caf/scheduler/worker.hpp"
#include "caf/scheduler/abstract_coordinator.hpp"

namespace caf {
namespace scheduler {

/// Policy-based implementation of the abstract coordinator base class.
template <class Policy>
class coordinator : public abstract_coordinator {
public:
  using super = abstract_coordinator;

  using policy_data = typename Policy::coordinator_data;

  coordinator(size_t nw = std::max(std::thread::hardware_concurrency(), 4u),
              size_t mt = std::numeric_limits<size_t>::max())
    : super(nw),
      max_throughput_(mt) {
    // nop
  }

  using worker_type = worker<Policy>;

  worker_type* worker_by_id(size_t id) {//override {
    return workers_[id].get();
  }

  policy_data& data() {
    return data_;
  }

protected:
  void initialize() override {
    super::initialize();
    // create workers
    workers_.resize(num_workers());
    for (size_t i = 0; i < num_workers(); ++i) {
      auto& ref = workers_[i];
      ref.reset(new worker_type(i, this, max_throughput_));
    }
    // start all workers now that all workers have been initialized
    for (auto& w : workers_) {
      w->start();
    }
  }

  void stop() override {
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
        CAF_ASSERT(ptr != nullptr);
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
    // use a set to keep track of remaining workers
    shutdown_helper sh;
    std::set<worker_type*> alive_workers;
    auto num = num_workers();
    for (size_t i = 0; i < num; ++i) {
      alive_workers.insert(worker_by_id(i));
    }
    CAF_LOG_DEBUG("enqueue shutdown_helper into each worker");
    while (! alive_workers.empty()) {
      (*alive_workers.begin())->external_enqueue(&sh);
      // since jobs can be stolen, we cannot assume that we have
      // actually shut down the worker we've enqueued sh to
      { // lifetime scope of guard
        std::unique_lock<std::mutex> guard(sh.mtx);
        sh.cv.wait(guard, [&] { return sh.last_worker != nullptr; });
      }
      alive_workers.erase(static_cast<worker_type*>(sh.last_worker));
      sh.last_worker = nullptr;
    }
    // shutdown utility actors
    stop_actors();
    // wait until all workers are done
    for (auto& w : workers_) {
      w->get_thread().join();
    }
    // run cleanup code for each resumable
    auto f = [](resumable* job) {
      job->detach_from_scheduler();
    };
    for (auto& w : workers_) {
      policy_.foreach_resumable(w.get(), f);
    }
    policy_.foreach_central_resumable(this, f);
  }

  void enqueue(resumable* ptr) {
    policy_.central_enqueue(this, ptr);
  }

private:
  // usually of size std::thread::hardware_concurrency()
  std::vector<std::unique_ptr<worker_type>> workers_;
  // policy-specific data
  policy_data data_;
  // instance of our policy object
  Policy policy_;
  // number of messages each actor is allowed to consume per resume
  size_t max_throughput_;
};

} // namespace scheduler
} // namespace caf

#endif // CAF_SCHEDULER_COORDINATOR_HPP
