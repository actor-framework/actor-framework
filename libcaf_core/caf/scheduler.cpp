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
#include "caf/detail/double_ended_queue.hpp"
#include "caf/detail/thread_safe_actor_clock.hpp"
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

// -- printer utilities --------------------------------------------------------

class actor_local_printer_impl : public detail::actor_local_printer {
public:
  actor_local_printer_impl(abstract_actor* self, actor printer)
    : self_(self->id()), printer_(std::move(printer)) {
    CAF_ASSERT(printer_ != nullptr);
    if (!self->getf(abstract_actor::has_used_aout_flag))
      self->setf(abstract_actor::has_used_aout_flag);
  }

  void write(std::string&& arg) override {
    printer_->enqueue(make_mailbox_element(nullptr, make_message_id(),
                                           add_atom_v, self_, std::move(arg)),
                      nullptr);
  }

  void write(const char* arg) override {
    write(std::string{arg});
  }

  void flush() override {
    printer_->enqueue(make_mailbox_element(nullptr, make_message_id(),
                                           flush_atom_v, self_),
                      nullptr);
  }

private:
  actor_id self_;
  actor printer_;
};

using string_sink = std::function<void(std::string&&)>;

using string_sink_ptr = std::shared_ptr<string_sink>;

using sink_cache = std::map<std::string, string_sink_ptr>;

string_sink make_sink(actor_system&, const std::string& fn, int flags) {
  if (fn.empty()) {
    return nullptr;
  } else if (fn.front() == ':') {
    // TODO: re-implement "virtual files" or remove
    return nullptr;
  } else {
    auto append = (flags & actor_ostream::append) != 0;
    auto fs = std::make_shared<std::ofstream>();
    fs->open(fn, append ? std::ios_base::out | std::ios_base::app
                        : std::ios_base::out);
    if (fs->is_open()) {
      return [fs](std::string&& out) { *fs << out; };
    } else {
      std::cerr << "cannot open file: " << fn << std::endl;
      return nullptr;
    }
  }
}

string_sink_ptr get_or_add_sink_ptr(actor_system& sys, sink_cache& fc,
                                    const std::string& fn, int flags) {
  if (auto i = fc.find(fn); i != fc.end()) {
    return i->second;
  } else if (auto fs = make_sink(sys, fn, flags)) {
    if (fs) {
      auto ptr = std::make_shared<string_sink>(std::move(fs));
      fc.emplace(fn, ptr);
      return ptr;
    } else {
      return nullptr;
    }
  } else {
    return nullptr;
  }
}

class printer_actor : public blocking_actor {
public:
  printer_actor(actor_config& cfg) : blocking_actor(cfg) {
    // nop
  }

  void act() override {
    struct actor_data {
      std::string current_line;
      string_sink_ptr redirect;
      actor_data() {
        // nop
      }
    };
    using data_map = std::unordered_map<actor_id, actor_data>;
    sink_cache fcache;
    string_sink_ptr global_redirect;
    data_map data;
    auto get_data = [&](actor_id addr, bool insert_missing) -> actor_data* {
      if (addr == invalid_actor_id)
        return nullptr;
      auto i = data.find(addr);
      if (i == data.end() && insert_missing)
        i = data.emplace(addr, actor_data{}).first;
      if (i != data.end())
        return &(i->second);
      return nullptr;
    };
    auto flush = [&](actor_data* what, bool forced) {
      if (what == nullptr)
        return;
      auto& line = what->current_line;
      if (line.empty() || (line.back() != '\n' && !forced))
        return;
      if (what->redirect)
        (*what->redirect)(std::move(line));
      else if (global_redirect)
        (*global_redirect)(std::move(line));
      else
        std::cout << line << std::flush;
      line.clear();
    };
    bool done = false;
    do_receive(
      [&](add_atom, actor_id aid, std::string& str) {
        if (str.empty() || aid == invalid_actor_id)
          return;
        auto d = get_data(aid, true);
        if (d != nullptr) {
          d->current_line += str;
          flush(d, false);
        }
      },
      [&](flush_atom, actor_id aid) { flush(get_data(aid, false), true); },
      [&](delete_atom, actor_id aid) {
        auto data_ptr = get_data(aid, false);
        if (data_ptr != nullptr) {
          flush(data_ptr, true);
          data.erase(aid);
        }
      },
      [&](redirect_atom, const std::string& fn, int flag) {
        global_redirect = get_or_add_sink_ptr(system(), fcache, fn, flag);
      },
      [&](redirect_atom, actor_id aid, const std::string& fn, int flag) {
        auto d = get_data(aid, true);
        if (d != nullptr)
          d->redirect = get_or_add_sink_ptr(system(), fcache, fn, flag);
      },
      [&](exit_msg& em) {
        fail_state(std::move(em.reason));
        done = true;
      })
      .until([&] { return done; });
  }

  const char* name() const override {
    return "printer_actor";
  }
};

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

  explicit worker_data(scheduler* p)
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
class worker : public execution_unit {
public:
  using job_ptr = resumable*;

  worker(size_t worker_id, scheduler* worker_parent, const worker_data& init,
         size_t throughput)
    : execution_unit(&worker_parent->system()),
      max_throughput_(throughput),
      id_(worker_id),
      data_(init) {
    // nop
  }

  worker(const worker&) = delete;

  worker& operator=(const worker&) = delete;

  template <class Parent>
  void start(Parent* parent) {
    CAF_ASSERT(this_thread_.get_id() == std::thread::id{});
    this_thread_ = system().launch_thread("caf.worker", thread_owner::scheduler,
                                          [this, parent] { run(parent); });
  }

  /// Enqueues a new job to the worker's queue from an external
  /// source, i.e., from any other thread.
  void external_enqueue(job_ptr job) {
    CAF_ASSERT(job != nullptr);
    data_.queue.append(job);
  }

  /// Enqueues a new job to the worker's queue from an internal
  /// source, i.e., a job that is currently executed by this worker.
  /// @warning Must not be called from other threads.
  void exec_later(job_ptr job) override {
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
    CAF_SET_LOGGER_SYS(&system());
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
  using super = scheduler;

  scheduler_impl(actor_system& sys) : super(sys) {
    // nop
  }

  using worker_type = worker;

  worker_type* worker_by_id(size_t x) {
    return workers_[x].get();
  }

  // -- implementation of scheduler interface ----------------------------------

  void enqueue(resumable* ptr) override {
    auto w = this->worker_by_id(next_worker++ % this->num_workers());
    w->external_enqueue(ptr);
  }

  detail::thread_safe_actor_clock& clock() noexcept override {
    return clock_;
  }

  void start() override {
    // Create initial state for all workers.
    worker_data init{this};
    // Prepare workers vector.
    auto num = num_workers();
    workers_.reserve(num);
    // Create worker instances.
    for (size_t i = 0; i < num; ++i)
      workers_.emplace_back(
        std::make_unique<worker_type>(i, this, init, max_throughput_));
    // Start all workers.
    for (auto& w : workers_)
      w->start(this);
    // Run remaining startup code.
    clock_.start_dispatch_loop(system());
    super::start();
  }

  void stop() override {
    // Shutdown workers.
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
    // Use a set to keep track of remaining workers.
    shutdown_helper sh;
    std::set<worker_type*> alive_workers;
    auto num = num_workers();
    for (size_t i = 0; i < num; ++i) {
      alive_workers.insert(worker_by_id(i));
      sh.ref(); // Make sure reference count is high enough.
    }
    while (!alive_workers.empty()) {
      (*alive_workers.begin())->external_enqueue(&sh);
      // Since jobs can be stolen, we cannot assume that we have actually shut
      // down the worker we've enqueued sh to.
      {
        std::unique_lock<std::mutex> guard(sh.mtx);
        sh.cv.wait(guard, [&] { return sh.last_worker != nullptr; });
      }
      alive_workers.erase(static_cast<worker_type*>(sh.last_worker));
      sh.last_worker = nullptr;
    }
    // Shutdown utility actors.
    stop_actors();
    // Wait until all workers are done.
    for (auto& w : workers_) {
      w->get_thread().join();
    }
    // Run cleanup code for each resumable.
    auto f = &scheduler::cleanup_and_release;
    for (auto& w : workers_) {
      auto next = [&] { return w->data().queue.try_take_head(); };
      for (auto job = next(); job != nullptr; job = next())
        f(job);
    }
    // Stop timer thread.
    clock_.stop_dispatch_loop();
  }

private:
  /// System-wide clock.
  detail::thread_safe_actor_clock clock_;

  /// Set of workers.
  std::vector<std::unique_ptr<worker_type>> workers_;

  /// Next worker.
  std::atomic<size_t> next_worker = 0;

  /// Thread for managing timeouts and delayed messages.
  std::thread timer_;
};

} // namespace work_stealing

// -- work sharing scheduler implementation ------------------------------------
namespace work_sharing {

/// Implementation of the work sharing worker class.
template <class Parent>
class worker : public execution_unit {
public:
  using job_ptr = resumable*;

  worker(size_t worker_id, Parent* worker_parent, size_t throughput)
    : execution_unit(&worker_parent->system()),
      max_throughput_(throughput),
      parent_{worker_parent},
      id_(worker_id) {
    // nop
  }

  worker(const worker&) = delete;

  worker& operator=(const worker&) = delete;

  void start() {
    CAF_ASSERT(this_thread_.get_id() == std::thread::id{});
    this_thread_ = system().launch_thread("caf.worker", thread_owner::scheduler,
                                          [this] { run(); });
  }

  /// Enqueues a new job to the worker's queue from an external
  /// source, i.e., from any other thread.
  void external_enqueue(job_ptr job) {
    CAF_ASSERT(job != nullptr);
    parent_->enqueue(job);
  }

  /// Enqueues a new job to the worker's queue from an internal
  /// source, i.e., a job that is currently executed by this worker.
  /// @warning Must not be called from other threads.
  void exec_later(job_ptr job) override {
    CAF_ASSERT(job != nullptr);
    parent_->enqueue(job);
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
    CAF_SET_LOGGER_SYS(&system());
    // scheduling loop
    for (;;) {
      auto job = parent_->dequeue();
      CAF_ASSERT(job != nullptr);
      CAF_ASSERT(job->subtype() != resumable::io_actor);
      auto res = job->resume(this, max_throughput_);
      switch (res) {
        case resumable::resume_later:
          // Keep reference to this actor, as it remains in the "loop".
          parent_->enqueue(job);
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

  // A thread-safe queue implementation.
  using queue_type = std::list<resumable*>;

  scheduler_impl(actor_system& sys) : super(sys) {
    // nop
  }

  void enqueue(resumable* ptr) override {
    queue_type l;
    l.push_back(ptr);
    std::unique_lock<std::mutex> guard(lock);
    queue.splice(queue.end(), l);
    cv.notify_one();
  }

  detail::thread_safe_actor_clock& clock() noexcept override {
    return clock_;
  }

  void start() override {
    // Prepare workers vector.
    auto num = num_workers();
    workers_.reserve(num);
    // Create worker instances.
    for (size_t i = 0; i < num; ++i)
      workers_.emplace_back(
        std::make_unique<worker_type>(i, this, max_throughput_));
    // Start all workers.
    for (auto& w : workers_)
      w->start();
    // Run remaining startup code.
    clock_.start_dispatch_loop(system());
    super::start();
  }

  void stop() override {
    // Shutdown workers.
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
    // Use a set to keep track of remaining workers.
    shutdown_helper sh;
    std::set<worker_type*> alive_workers;
    auto num = num_workers();
    for (size_t i = 0; i < num; ++i) {
      alive_workers.insert(worker_by_id(i));
      sh.ref(); // Make sure reference count is high enough.
    }
    while (!alive_workers.empty()) {
      (*alive_workers.begin())->external_enqueue(&sh);
      // Since jobs can be stolen, we cannot assume that we have actually shut
      // down the worker we've enqueued sh to.
      { // lifetime scope of guard
        std::unique_lock<std::mutex> guard(sh.mtx);
        sh.cv.wait(guard, [&] { return sh.last_worker != nullptr; });
      }
      alive_workers.erase(static_cast<worker_type*>(sh.last_worker));
      sh.last_worker = nullptr;
    }
    // Shutdown utility actors.
    stop_actors();
    // Wait until all workers are done.
    for (auto& w : workers_) {
      w->get_thread().join();
    }
    // Run cleanup code for each resumable.
    auto f = &scheduler::cleanup_and_release;
    foreach_central_resumable(f);
    // Stop timer thread.
    clock_.stop_dispatch_loop();
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

  /// System-wide clock.
  detail::thread_safe_actor_clock clock_;

  /// Set of workers.
  std::vector<std::unique_ptr<worker_type>> workers_;

  queue_type queue;

  std::mutex lock;

  std::condition_variable cv;

  /// Thread for managing timeouts and delayed messages.
  std::thread timer_;
};

} // namespace work_sharing

} // namespace

// -- factory functions ------------------------------------------------------

std::unique_ptr<scheduler> scheduler::make_work_stealing(actor_system& sys) {
  return std::make_unique<work_stealing::scheduler_impl>(sys);
}

std::unique_ptr<scheduler> scheduler::make_work_sharing(actor_system& sys) {
  return std::make_unique<work_sharing::scheduler_impl>(sys);
}

// -- constructors -------------------------------------------------------------

scheduler::scheduler(actor_system& sys)
  : max_throughput_(0), num_workers_(0), system_(sys) {
  // nop
}

scheduler::~scheduler() {
  // nop
}

// -- default implementation of scheduler --------------------------------------

void scheduler::start() {
  CAF_LOG_TRACE("");
  // Launch print utility actors.
  system_.printer(system_.spawn<printer_actor, hidden + detached>());
}

// -- utility functions --------------------------------------------------------

void scheduler::init(actor_system_config& cfg) {
  namespace sr = defaults::scheduler;
  max_throughput_ = get_or(cfg, "caf.scheduler.max-throughput",
                           sr::max_throughput);
  num_workers_ = get_or(cfg, "caf.scheduler.max-threads",
                        default_thread_count());
}

detail::actor_local_printer_ptr scheduler::printer_for(local_actor* self) {
  return make_counted<actor_local_printer_impl>(self, system_.printer());
}

size_t scheduler::default_thread_count() noexcept {
  return std::max(std::thread::hardware_concurrency(), 4u);
}

void scheduler::stop_actors() {
  CAF_LOG_TRACE("");
  scoped_actor self{system_, true};
  anon_send_exit(system_.printer(), exit_reason::user_shutdown);
  self->wait_for(system_.printer());
}

void scheduler::cleanup_and_release(resumable* ptr) {
  class dummy_unit : public execution_unit {
  public:
    dummy_unit(local_actor* job) : execution_unit(&job->home_system()) {
      // nop
    }
    void exec_later(resumable* job) override {
      resumables.push_back(job);
    }
    std::vector<resumable*> resumables;
  };
  switch (ptr->subtype()) {
    case resumable::scheduled_actor:
    case resumable::io_actor: {
      auto dptr = static_cast<scheduled_actor*>(ptr);
      dummy_unit dummy{dptr};
      dptr->cleanup(make_error(exit_reason::user_shutdown), &dummy);
      while (!dummy.resumables.empty()) {
        auto sub = dummy.resumables.back();
        dummy.resumables.pop_back();
        switch (sub->subtype()) {
          case resumable::scheduled_actor:
          case resumable::io_actor: {
            auto dsub = static_cast<scheduled_actor*>(sub);
            dsub->cleanup(make_error(exit_reason::user_shutdown), &dummy);
            break;
          }
          default:
            break;
        }
      }
      break;
    }
    default:
      break;
  }
  intrusive_ptr_release(ptr);
}

} // namespace caf
