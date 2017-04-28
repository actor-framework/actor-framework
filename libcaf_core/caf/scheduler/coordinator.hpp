/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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

#include "caf/config.hpp"

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

  using worker_type = worker<Policy>;
  using policy_data = typename Policy::template coordinator_data<worker_type>;

  coordinator(actor_system& sys) : super(sys), data_(this) {
    // nop
  }

  /// return a worker by its vector index 
  /// (this is not necessarily the id of the worker)
  worker_type* worker_by_idx(size_t x) {
    return data_.workers[x].get();
  }

  policy_data& data() {
    return data_;
  }

  static actor_system::module* make(actor_system& sys, detail::type_list<>) {
    return new coordinator(sys);
  }

protected:
  void start() override {
    // initialize workers vector
    policy_.template create_workers<coordinator<Policy>, worker_type>(this, num_workers(), max_throughput_);
    // start all workers now that all workers have been initialized
    for (auto& w : data_.workers)
      w->start();
    // run remaining startup code
    super::start();
  }

  void stop() override {
    // shutdown workers
    class shutdown_helper : public resumable, public ref_counted {
    public:
      resumable::resume_result resume(execution_unit* ptr, size_t) override {
        CAF_ASSERT(ptr != nullptr);
        std::unique_lock<std::mutex> guard(mtx);
        last_worker = ptr;
        cv.notify_all();
        return resumable::shutdown_execution_unit;
      }
      void intrusive_ptr_add_ref_impl() override {
        intrusive_ptr_add_ref(this);
      }

      void intrusive_ptr_release_impl() override {
        intrusive_ptr_release(this);
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
      alive_workers.insert(worker_by_idx(i));
      sh.ref(); // make sure reference count is high enough
    }
    while (!alive_workers.empty()) {
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
    for (auto& w : data_.workers) {
      w->get_thread().join();
    }
    // run cleanup code for each resumable
    auto f = &abstract_coordinator::cleanup_and_release;
    for (auto& w : data_.workers)
      policy_.foreach_resumable(w.get(), f);
    policy_.foreach_central_resumable(this, f);
  }

  void enqueue(resumable* ptr) override {
    policy_.central_enqueue(this, ptr);
  }

private:
  // policy-specific data
  policy_data data_;
  // instance of our policy object
  Policy policy_;
};

} // namespace scheduler
} // namespace caf

#endif // CAF_SCHEDULER_COORDINATOR_HPP
