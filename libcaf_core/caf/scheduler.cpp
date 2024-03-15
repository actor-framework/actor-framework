// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/scheduler.hpp"

#include "caf/actor_ostream.hpp"
#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/blocking_actor.hpp"
#include "caf/config.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/cleanup_and_release.hpp"
#include "caf/detail/default_thread_count.hpp"
#include "caf/detail/double_ended_queue.hpp"
#include "caf/logger.hpp"
#include "caf/scheduled_actor.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/send.hpp"
#include "caf/thread_owner.hpp"

#include <condition_variable>
#include <fstream>
#include <ios>
#include <iostream>
#include <memory>
#include <random>
#include <thread>

namespace caf {

// -- utility and implementation details ---------------------------------------

namespace {

// -- work stealing scheduler implementation -----------------------------------

namespace work_stealing {

// Holds job queue of a worker and a random number generator.
struct worker_data {
  // Configuration for aggressive/moderate/relaxed poll strategies.
  struct poll_strategy {
    size_t attempts;
    size_t step_size;
    size_t steal_interval;
    timespan sleep_duration;
  };

  template <class SchedulerImpl>
  explicit worker_data(SchedulerImpl* p)
    : rengine(std::random_device{}()),
      // No need to worry about wrap-around; if `p->num_workers() < 2`,
      // `uniform` will not be used anyway.
      uniform(0, p->num_workers() - 2),
      strategies{
        {{get_or(p->config(), "caf.work-stealing.aggressive-poll-attempts",
                 defaults::work_stealing::aggressive_poll_attempts),
          1,
          get_or(p->config(), "caf.work-stealing.aggressive-steal-interval",
                 defaults::work_stealing::aggressive_steal_interval),
          timespan{0}},
         {get_or(p->config(), "caf.work-stealing.moderate-poll-attempts",
                 defaults::work_stealing::moderate_poll_attempts),
          1,
          get_or(p->config(), "caf.work-stealing.moderate-steal-interval",
                 defaults::work_stealing::moderate_steal_interval),
          get_or(p->config(), "caf.work-stealing.moderate-sleep-duration",
                 defaults::work_stealing::moderate_sleep_duration)},
         {1, 0,
          get_or(p->config(), "caf.work-stealing.relaxed-steal-interval",
                 defaults::work_stealing::relaxed_steal_interval),
          get_or(p->config(), "caf.work-stealing.relaxed-sleep-duration",
                 defaults::work_stealing::relaxed_sleep_duration)}}} {
    // nop
  }

  worker_data(const worker_data& other)
    : rengine(std::random_device{}()),
      uniform(other.uniform),
      strategies(other.strategies) {
    // nop
  }

  // This queue is exposed to other workers that may attempt to steal jobs
  // from it and the central scheduling unit can push new jobs to the queue.
  detail::double_ended_queue<resumable> queue;

  // Needed to generate pseudo random numbers.
  std::default_random_engine rengine;
  std::uniform_int_distribution<size_t> uniform;
  std::array<poll_strategy, 3> strategies;
};

/// Implementation of the work stealing worker class.
class worker : public scheduler {
public:
  using job_ptr = resumable*;

  template <class SchedulerImpl>
  worker(size_t worker_id, SchedulerImpl*, const worker_data& init,
         size_t throughput)
    : max_throughput_(throughput), id_(worker_id), data_(init) {
    // nop
  }

  worker(const worker&) = delete;

  worker& operator=(const worker&) = delete;

  template <class Parent>
  void start(Parent* parent) {
    CAF_ASSERT(this_thread_.get_id() == std::thread::id{});
    this_thread_ = parent->system().launch_thread(
      "caf.worker", thread_owner::scheduler, [this, parent] { run(parent); });
  }

  void start() override {
    // nop
  }

  void stop() override {
    // nop
  }

  void schedule(job_ptr job) override {
    CAF_ASSERT(job != nullptr);
    data_.queue.append(job);
  }

  void delay(job_ptr job) override {
    CAF_ASSERT(job != nullptr);
    data_.queue.prepend(job);
  }

  size_t id() const {
    return id_;
  }

  std::thread& get_thread() {
    return this_thread_;
  }

  worker_data& data() {
    return data_;
  }

  size_t max_throughput() {
    return max_throughput_;
  }

private:
  // Goes on a raid in quest for a shiny new job.
  template <typename Parent>
  resumable* try_steal(Parent* p) {
    if (p->num_workers() < 2) {
      // You can't steal from yourthis, can you?
      return nullptr;
    }
    // Roll the dice to pick a victim other than ourselves.
    auto victim = data_.uniform(data_.rengine);
    if (victim == this->id())
      victim = p->num_workers() - 1;
    // Steal oldest element from the victim's queue.
    return p->worker_by_id(victim)->data_.queue.try_take_tail();
  }

  template <typename Parent>
  resumable* policy_dequeue(Parent* parent) {
    // We wait for new jobs by polling our external queue: first, we assume an
    // active work load on the machine and perform aggressive polling, then we
    // relax our polling a bit and wait 50us between dequeue attempts.
    auto& strategies = data_.strategies;
    auto* job = data_.queue.try_take_head();
    if (job)
      return job;
    for (size_t k = 0; k < 2; ++k) { // Iterate over the first two strategies.
      for (size_t i = 0; i < strategies[k].attempts;
           i += strategies[k].step_size) {
        // try to steal every X poll attempts
        if ((i % strategies[k].steal_interval) == 0) {
          job = try_steal(parent);
          if (job)
            return job;
        }
        // Wait for some work to appear.
        job = data_.queue.try_take_head(strategies[k].sleep_duration);
        if (job)
          return job;
      }
    }
    // We assume pretty much nothing is going on so we can relax polling
    // and falling to sleep on a condition variable whose timeout is the one
    // of the relaxed polling strategy
    auto& relaxed = strategies[2];
    do {
      job = data_.queue.try_take_head(relaxed.sleep_duration);
    } while (job == nullptr);
    return job;
  }

  template <typename Parent>
  void run(Parent* parent) {
    CAF_SET_LOGGER_SYS(&parent->system());
    // scheduling loop
    for (;;) {
      auto job = policy_dequeue(parent);
      CAF_ASSERT(job != nullptr);
      CAF_ASSERT(job->subtype() != resumable::io_actor);
      auto res = job->resume(this, max_throughput_);
      switch (res) {
        case resumable::resume_later: {
          // Keep reference to this actor, as it remains in the "loop" job has
          // voluntarily released the CPU to let others run instead this means
          // we are going to put this job to the very end of our queue.
          data_.queue.unsafe_append(job);
          break;
        }
        case resumable::done: {
          intrusive_ptr_release(job);
          break;
        }
        case resumable::awaiting_message: {
          // Resumable will maybe be enqueued again later, deref it for now.
          intrusive_ptr_release(job);
          break;
        }
        case resumable::shutdown_execution_unit: {
          return;
        }
      }
    }
  }
  // Number of messages each actor is allowed to consume per resume.
  size_t max_throughput_;

  // The worker's thread.
  std::thread this_thread_;

  // The worker's ID received from scheduler.
  size_t id_;

  // Policy-specific data.
  worker_data data_;
};

/// Policy-based implementation of the scheduler base class.
class scheduler_impl : public scheduler {
public:
  explicit scheduler_impl(actor_system& sys) : sys_(&sys) {
    auto& cfg = sys.config();
    max_throughput_ = get_or(cfg, "caf.scheduler.max-throughput",
                             defaults::scheduler::max_throughput);
    num_workers_ = get_or(cfg, "caf.scheduler.max-threads",
                          detail::default_thread_count());
  }

  using worker_type = worker;

  worker_type* worker_by_id(size_t x) {
    return workers_[x].get();
  }

  // -- properties -------------------------------------------------------------

  actor_system& system() {
    return *sys_;
  }

  const actor_system_config& config() {
    return sys_->config();
  }

  size_t num_workers() const noexcept {
    return num_workers_;
  }

  // -- implementation of scheduler interface ----------------------------------

  void schedule(resumable* ptr) override {
    auto w = this->worker_by_id(next_worker++ % num_workers_);
    w->schedule(ptr);
  }

  void delay(resumable* what) override {
    schedule(what);
  }

  void start() override {
    // Create initial state for all workers.
    worker_data init{this};
    // Prepare workers vector.
    workers_.reserve(num_workers_);
    // Create worker instances.
    for (size_t i = 0; i < num_workers_; ++i)
      workers_.emplace_back(
        std::make_unique<worker_type>(i, this, init, max_throughput_));
    // Start all workers.
    for (auto& w : workers_)
      w->start(this);
  }

  void stop() override {
    // Shutdown workers.
    class shutdown_helper : public resumable, public ref_counted {
    public:
      resumable::resume_result resume(scheduler* ptr, size_t) override {
        CAF_ASSERT(ptr != nullptr);
        std::unique_lock<std::mutex> guard(mtx);
        last_worker = ptr;
        cv.notify_all();
        return resumable::shutdown_execution_unit;
      }

      void ref_resumable() const noexcept final {
        ref();
      }

      void deref_resumable() const noexcept final {
        deref();
      }
      shutdown_helper() : last_worker(nullptr) {
        // nop
      }
      std::mutex mtx;
      std::condition_variable cv;
      scheduler* last_worker;
    };
    // Use a set to keep track of remaining workers.
    shutdown_helper sh;
    std::set<worker_type*> alive_workers;
    for (size_t i = 0; i < num_workers_; ++i) {
      alive_workers.insert(worker_by_id(i));
      sh.ref(); // Make sure reference count is high enough.
    }
    while (!alive_workers.empty()) {
      (*alive_workers.begin())->schedule(&sh);
      // Since jobs can be stolen, we cannot assume that we have actually shut
      // down the worker we've enqueued sh to.
      {
        std::unique_lock<std::mutex> guard(sh.mtx);
        sh.cv.wait(guard, [&] { return sh.last_worker != nullptr; });
      }
      alive_workers.erase(static_cast<worker_type*>(sh.last_worker));
      sh.last_worker = nullptr;
    }
    // Wait until all workers are done.
    for (auto& w : workers_) {
      w->get_thread().join();
    }
    // Run cleanup code for each resumable.
    for (auto& w : workers_) {
      auto next = [&] { return w->data().queue.try_take_head(); };
      for (auto job = next(); job != nullptr; job = next())
        detail::cleanup_and_release(job);
    }
  }

private:
  /// Set of workers.
  std::vector<std::unique_ptr<worker_type>> workers_;

  /// Next worker.
  std::atomic<size_t> next_worker = 0;

  /// Thread for managing timeouts and delayed messages.
  std::thread timer_;

  /// Number of messages each actor is allowed to consume per resume.
  size_t max_throughput_ = 0;

  /// Configured number of workers.
  size_t num_workers_ = 0;

  /// Reference to the host system.
  actor_system* sys_ = nullptr;
};

} // namespace work_stealing

// -- work sharing scheduler implementation ------------------------------------

namespace work_sharing {

/// Implementation of the work sharing worker class.
template <class Parent>
class worker : public scheduler {
public:
  using job_ptr = resumable*;

  worker(size_t worker_id, Parent* parent, size_t throughput)
    : max_throughput_(throughput), parent_{parent}, id_(worker_id) {
    // nop
  }

  worker(const worker&) = delete;

  worker& operator=(const worker&) = delete;

  void start() override {
    CAF_ASSERT(this_thread_.get_id() == std::thread::id{});
    this_thread_ = parent_->system().launch_thread("caf.worker",
                                                   thread_owner::scheduler,
                                                   [this] { run(); });
  }

  void stop() override {
    // nop
  }

  void schedule(job_ptr job) override {
    CAF_ASSERT(job != nullptr);
    parent_->schedule(job);
  }

  void delay(job_ptr job) override {
    CAF_ASSERT(job != nullptr);
    parent_->schedule(job);
  }

  size_t id() const noexcept {
    return id_;
  }

  std::thread& get_thread() noexcept {
    return this_thread_;
  }

  size_t max_throughput() noexcept {
    return max_throughput_;
  }

private:
  void run() {
    CAF_SET_LOGGER_SYS(&parent_->system());
    // scheduling loop
    for (;;) {
      auto job = parent_->dequeue();
      CAF_ASSERT(job != nullptr);
      CAF_ASSERT(job->subtype() != resumable::io_actor);
      auto res = job->resume(this, max_throughput_);
      switch (res) {
        case resumable::resume_later:
          // Keep reference to this actor, as it remains in the "loop".
          parent_->schedule(job);
          break;
        case resumable::awaiting_message:
          // Resumable will maybe be enqueued again later, deref it for now.
        case resumable::done:
          intrusive_ptr_release(job);
          break;
        case resumable::shutdown_execution_unit: {
          return;
        }
      }
    }
  }

  // Number of messages each actor is allowed to consume per resume.
  size_t max_throughput_;

  // The worker's thread.
  std::thread this_thread_;

  // Pointer to the scheduler.
  Parent* parent_;

  // The worker's ID received from scheduler.
  size_t id_;
};

class scheduler_impl : public scheduler {
public:
  using super = scheduler;

  using worker_type = worker<scheduler_impl>;

  using queue_type = std::list<resumable*>;

  explicit scheduler_impl(actor_system& sys) : sys_(&sys) {
    auto& cfg = sys.config();
    max_throughput_ = get_or(cfg, "caf.scheduler.max-throughput",
                             defaults::scheduler::max_throughput);
    num_workers_ = get_or(cfg, "caf.scheduler.max-threads",
                          detail::default_thread_count());
  }

  // -- properties -------------------------------------------------------------

  actor_system& system() {
    return *sys_;
  }

  const actor_system_config& config() {
    return sys_->config();
  }

  size_t num_workers() const noexcept {
    return num_workers_;
  }

  // -- implementation of scheduler interface ----------------------------------

  void schedule(resumable* ptr) override {
    queue_type l;
    l.push_back(ptr);
    std::unique_lock<std::mutex> guard(lock);
    queue.splice(queue.end(), l);
    cv.notify_one();
  }

  void delay(resumable* what) override {
    schedule(what);
  }

  void start() override {
    // Prepare workers vector.
    auto num = num_workers_;
    workers_.reserve(num);
    // Create worker instances.
    for (size_t i = 0; i < num; ++i)
      workers_.emplace_back(
        std::make_unique<worker_type>(i, this, max_throughput_));
    // Start all workers.
    for (auto& w : workers_)
      w->start();
  }

  void stop() override {
    // Shutdown workers.
    class shutdown_helper : public resumable, public ref_counted {
    public:
      resumable::resume_result resume(scheduler* ptr, size_t) override {
        CAF_ASSERT(ptr != nullptr);
        std::unique_lock<std::mutex> guard(mtx);
        last_worker = ptr;
        cv.notify_all();
        return resumable::shutdown_execution_unit;
      }
      void ref_resumable() const noexcept final {
        ref();
      }
      void deref_resumable() const noexcept final {
        deref();
      }
      shutdown_helper() : last_worker(nullptr) {
        // nop
      }
      std::mutex mtx;
      std::condition_variable cv;
      scheduler* last_worker;
    };
    // Use a set to keep track of remaining workers.
    shutdown_helper sh;
    std::set<worker_type*> alive_workers;
    for (size_t i = 0; i < num_workers_; ++i) {
      alive_workers.insert(worker_by_id(i));
      sh.ref(); // Make sure reference count is high enough.
    }
    while (!alive_workers.empty()) {
      (*alive_workers.begin())->schedule(&sh);
      // Since jobs can be stolen, we cannot assume that we have actually shut
      // down the worker we've enqueued sh to.
      { // lifetime scope of guard
        std::unique_lock<std::mutex> guard(sh.mtx);
        sh.cv.wait(guard, [&] { return sh.last_worker != nullptr; });
      }
      alive_workers.erase(static_cast<worker_type*>(sh.last_worker));
      sh.last_worker = nullptr;
    }
    // Wait until all workers are done.
    for (auto& w : workers_) {
      w->get_thread().join();
    }
    // Run cleanup code for each resumable.
    foreach_central_resumable(detail::cleanup_and_release);
  }

  resumable* dequeue() {
    std::unique_lock<std::mutex> guard(lock);
    cv.wait(guard, [&] { return !queue.empty(); });
    resumable* job = queue.front();
    queue.pop_front();
    return job;
  }

private:
  worker_type* worker_by_id(size_t x) {
    return workers_[x].get();
  }

  template <class UnaryFunction>
  void foreach_central_resumable(UnaryFunction f) {
    auto next = [&]() -> resumable* {
      if (queue.empty()) {
        return nullptr;
      }
      auto front = queue.front();
      queue.pop_front();
      return front;
    };
    std::unique_lock<std::mutex> guard(lock);
    for (auto job = next(); job != nullptr; job = next()) {
      f(job);
    }
  }

  /// Set of workers.
  std::vector<std::unique_ptr<worker_type>> workers_;

  queue_type queue;

  std::mutex lock;

  std::condition_variable cv;

  /// Thread for managing timeouts and delayed messages.
  std::thread timer_;

  /// Number of messages each actor is allowed to consume per resume.
  size_t max_throughput_ = 0;

  /// Configured number of workers.
  size_t num_workers_ = 0;

  /// Reference to the host system.
  actor_system* sys_ = nullptr;
};

} // namespace work_sharing

} // namespace

// -- factory functions --------------------------------------------------------

std::unique_ptr<scheduler> scheduler::make_work_stealing(actor_system& sys) {
  return std::make_unique<work_stealing::scheduler_impl>(sys);
}

std::unique_ptr<scheduler> scheduler::make_work_sharing(actor_system& sys) {
  return std::make_unique<work_sharing::scheduler_impl>(sys);
}

// -- constructors, destructors, and assignment operators ----------------------

scheduler::~scheduler() {
  // nop
}

} // namespace caf
