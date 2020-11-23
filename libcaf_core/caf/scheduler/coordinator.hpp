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

#include "caf/config.hpp"

#include <condition_variable>
#include <limits>
#include <memory>
#include <thread>

#include "caf/detail/set_thread_name.hpp"
#include "caf/detail/thread_safe_actor_clock.hpp"
#include "caf/scheduler/abstract_coordinator.hpp"
#include "caf/scheduler/worker.hpp"

namespace caf::scheduler {

/// Policy-based implementation of the abstract coordinator base class.
template <class Policy>
class coordinator : public abstract_coordinator {
public:
  using super = abstract_coordinator;

  using policy_data = typename Policy::coordinator_data;

  coordinator(actor_system& sys) : super(sys), data_(this) {
    // nop
  }

  using worker_type = worker<Policy>;

  worker_type* worker_by_id(size_t x) {
    return workers_[x].get();
  }

  policy_data& data() {
    return data_;
  }

  static actor_system::module* make(actor_system& sys, detail::type_list<>) {
    return new coordinator(sys);
  }

protected:
  void start() override {
    // Create initial state for all workers.
    typename worker_type::policy_data init{this};
    // Prepare workers vector.
    auto num = num_workers();
    workers_.reserve(num);
    // Create worker instanes.
    for (size_t i = 0; i < num; ++i)
      workers_.emplace_back(
        std::make_unique<worker_type>(i, this, init, max_throughput_));
    // Start all workers.
    for (auto& w : workers_)
      w->start();
    // Launch an additional background thread for dispatching timeouts and
    // delayed messages.
    timer_ = std::thread{[&] {
      CAF_SET_LOGGER_SYS(&system());
      detail::set_thread_name("caf.clock");
      system().thread_started();
      clock_.run_dispatch_loop();
      system().thread_terminates();
    }};
    // Run remaining startup code.
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
      alive_workers.insert(worker_by_id(i));
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
    for (auto& w : workers_) {
      w->get_thread().join();
    }
    // run cleanup code for each resumable
    auto f = &abstract_coordinator::cleanup_and_release;
    for (auto& w : workers_)
      policy_.foreach_resumable(w.get(), f);
    policy_.foreach_central_resumable(this, f);
    // stop timer thread
    clock_.cancel_dispatch_loop();
    timer_.join();
  }

  void enqueue(resumable* ptr) override {
    policy_.central_enqueue(this, ptr);
  }

  detail::thread_safe_actor_clock& clock() noexcept override {
    return clock_;
  }

private:
  /// System-wide clock.
  detail::thread_safe_actor_clock clock_;

  /// Set of workers.
  std::vector<std::unique_ptr<worker_type>> workers_;

  /// Policy-specific data.
  policy_data data_;

  /// The policy object.
  Policy policy_;

  /// Thread for managing timeouts and delayed messages.
  std::thread timer_;
};

} // namespace caf::scheduler
