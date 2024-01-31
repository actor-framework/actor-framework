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

// -- scheduler implementation -------------------------------------------------

template <class Policy>
class scheduler_impl;

/// Policy-based implementation of the abstract worker base class.
template <class Policy>
class worker : public execution_unit {
public:
  using job_ptr = resumable*;
  using scheduler_impl_ptr = scheduler_impl<Policy>*;
  using policy_data = typename Policy::worker_data;

  worker(size_t worker_id, scheduler_impl_ptr worker_parent,
         const policy_data& init, size_t throughput)
    : execution_unit(&worker_parent->system()),
      max_throughput_(throughput),
      id_(worker_id),
      parent_(worker_parent),
      data_(init) {
    // nop
  }

  void start() {
    CAF_ASSERT(this_thread_.get_id() == std::thread::id{});
    this_thread_ = system().launch_thread("caf.worker", thread_owner::scheduler,
                                          [this] { run(); });
  }

  worker(const worker&) = delete;
  worker& operator=(const worker&) = delete;

  /// Enqueues a new job to the worker's queue from an external
  /// source, i.e., from any other thread.
  void external_enqueue(job_ptr job) {
    CAF_ASSERT(job != nullptr);
    policy_.external_enqueue(this, job);
  }

  /// Enqueues a new job to the worker's queue from an internal
  /// source, i.e., a job that is currently executed by this worker.
  /// @warning Must not be called from other threads.
  void exec_later(job_ptr job) override {
    CAF_ASSERT(job != nullptr);
    policy_.internal_enqueue(this, job);
  }

  scheduler_impl_ptr parent() {
    return parent_;
  }

  size_t id() const {
    return id_;
  }

  std::thread& get_thread() {
    return this_thread_;
  }

  actor_id id_of(resumable* ptr) {
    abstract_actor* dptr = ptr != nullptr ? dynamic_cast<abstract_actor*>(ptr)
                                          : nullptr;
    return dptr != nullptr ? dptr->id() : 0;
  }

  policy_data& data() {
    return data_;
  }

  size_t max_throughput() {
    return max_throughput_;
  }

private:
  void run() {
    CAF_SET_LOGGER_SYS(&system());
    // scheduling loop
    for (;;) {
      auto job = policy_.dequeue(this);
      CAF_ASSERT(job != nullptr);
      CAF_ASSERT(job->subtype() != resumable::io_actor);
      auto res = job->resume(this, max_throughput_);
      switch (res) {
        case resumable::resume_later: {
          // keep reference to this actor, as it remains in the "loop"
          policy_.resume_job_later(this, job);
          break;
        }
        case resumable::done: {
          intrusive_ptr_release(job);
          break;
        }
        case resumable::awaiting_message: {
          // resumable will maybe be enqueued again later, deref it for now
          intrusive_ptr_release(job);
          break;
        }
        case resumable::shutdown_execution_unit: {
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
  scheduler_impl_ptr parent_;
  // policy-specific data
  policy_data data_;
  // instance of our policy object
  Policy policy_;
};

/// Policy-based implementation of the scheduler base class.
template <class Policy>
class scheduler_impl : public scheduler {
public:
  using super = scheduler;

  using policy_data = typename Policy::coordinator_data;

  scheduler_impl(actor_system& sys) : super(sys), data_(this) {
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
    return new scheduler_impl(sys);
  }

protected:
  void start() override {
    // Create initial state for all workers.
    typename worker_type::policy_data init{this};
    // Prepare workers vector.
    auto num = num_workers();
    workers_.reserve(num);
    // Create worker instances.
    for (size_t i = 0; i < num; ++i)
      workers_.emplace_back(
        std::make_unique<worker_type>(i, this, init, max_throughput_));
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
    for (auto& w : workers_)
      policy_.foreach_resumable(w.get(), f);
    policy_.foreach_central_resumable(this, f);
    // Stop timer thread.
    clock_.stop_dispatch_loop();
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

// Convenience function to access the data field.
template <class WorkerOrCoordinator>
auto d(WorkerOrCoordinator* self) -> decltype(self->data()) {
  return self->data();
}

/// Implements scheduling of actors via work stealing.
class work_stealing {
public:
  // A thread-safe queue implementation.
  using queue_type = detail::double_ended_queue<resumable>;

  // configuration for aggressive/moderate/relaxed poll strategies.
  struct poll_strategy {
    size_t attempts;
    size_t step_size;
    size_t steal_interval;
    timespan sleep_duration;
  };

  // The coordinator has only a counter for round-robin enqueue to its workers.
  struct coordinator_data {
    explicit coordinator_data(scheduler*) : next_worker(0) {
      // nop
    }

    std::atomic<size_t> next_worker;
  };

  // Holds job job queue of a worker and a random number generator.
  struct worker_data {
    explicit worker_data(scheduler* p)
      : rengine(std::random_device{}()),
        // no need to worry about wrap-around; if `p->num_workers() < 2`,
        // `uniform` will not be used anyway
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
    queue_type queue;
    // needed to generate pseudo random numbers
    std::default_random_engine rengine;
    std::uniform_int_distribution<size_t> uniform;
    std::array<poll_strategy, 3> strategies;
  };

  // Goes on a raid in quest for a shiny new job.
  template <class Worker>
  resumable* try_steal(Worker* self) {
    auto p = self->parent();
    if (p->num_workers() < 2) {
      // you can't steal from yourself, can you?
      return nullptr;
    }
    // roll the dice to pick a victim other than ourselves
    auto victim = d(self).uniform(d(self).rengine);
    if (victim == self->id())
      victim = p->num_workers() - 1;
    // steal oldest element from the victim's queue
    return d(p->worker_by_id(victim)).queue.try_take_tail();
  }

  template <class Coordinator>
  void central_enqueue(Coordinator* self, resumable* job) {
    auto w = self->worker_by_id(d(self).next_worker++ % self->num_workers());
    w->external_enqueue(job);
  }

  template <class Worker>
  void external_enqueue(Worker* self, resumable* job) {
    d(self).queue.append(job);
  }

  template <class Worker>
  void internal_enqueue(Worker* self, resumable* job) {
    d(self).queue.prepend(job);
  }

  template <class Worker>
  void resume_job_later(Worker* self, resumable* job) {
    // job has voluntarily released the CPU to let others run instead
    // this means we are going to put this job to the very end of our queue
    d(self).queue.unsafe_append(job);
  }

  template <class Worker>
  resumable* dequeue(Worker* self) {
    // we wait for new jobs by polling our external queue: first, we
    // assume an active work load on the machine and perform aggressive
    // polling, then we relax our polling a bit and wait 50 us between
    // dequeue attempts
    auto& strategies = d(self).strategies;
    auto* job = d(self).queue.try_take_head();
    if (job)
      return job;
    for (size_t k = 0; k < 2; ++k) { // iterate over the first two strategies
      for (size_t i = 0; i < strategies[k].attempts;
           i += strategies[k].step_size) {
        // try to steal every X poll attempts
        if ((i % strategies[k].steal_interval) == 0) {
          job = try_steal(self);
          if (job)
            return job;
        }
        // wait for some work to appear
        job = d(self).queue.try_take_head(strategies[k].sleep_duration);
        if (job)
          return job;
      }
    }
    // we assume pretty much nothing is going on so we can relax polling
    // and falling to sleep on a condition variable whose timeout is the one
    // of the relaxed polling strategy
    auto& relaxed = strategies[2];
    do {
      job = d(self).queue.try_take_head(relaxed.sleep_duration);
    } while (job == nullptr);
    return job;
  }

  template <class Worker, class UnaryFunction>
  void foreach_resumable(Worker* self, UnaryFunction f) {
    auto next = [&] { return d(self).queue.try_take_head(); };
    for (auto job = next(); job != nullptr; job = next()) {
      f(job);
    }
  }

  template <class Coordinator, class UnaryFunction>
  void foreach_central_resumable(Coordinator*, UnaryFunction) {
    // nop
  }
};

/// Implements scheduling of actors via work sharing (central job queue).
class work_sharing {
public:
  // A thread-safe queue implementation.
  using queue_type = std::list<resumable*>;

  struct coordinator_data {
    explicit coordinator_data(scheduler*) {
      // nop
    }

    queue_type queue;
    std::mutex lock;
    std::condition_variable cv;
  };

  struct worker_data {
    explicit worker_data(scheduler*) {
      // nop
    }
  };

  template <class Coordinator>
  bool enqueue(Coordinator* self, resumable* job) {
    queue_type l;
    l.push_back(job);
    std::unique_lock<std::mutex> guard(d(self).lock);
    d(self).queue.splice(d(self).queue.end(), l);
    d(self).cv.notify_one();
    return true;
  }

  template <class Coordinator>
  void central_enqueue(Coordinator* self, resumable* job) {
    enqueue(self, job);
  }

  template <class Worker>
  void external_enqueue(Worker* self, resumable* job) {
    enqueue(self->parent(), job);
  }

  template <class Worker>
  void internal_enqueue(Worker* self, resumable* job) {
    enqueue(self->parent(), job);
  }

  template <class Worker>
  void resume_job_later(Worker* self, resumable* job) {
    // job has voluntarily released the CPU to let others run instead
    // this means we are going to put this job to the very end of our queue
    enqueue(self->parent(), job);
  }

  template <class Worker>
  resumable* dequeue(Worker* self) {
    auto& parent_data = d(self->parent());
    std::unique_lock<std::mutex> guard(parent_data.lock);
    parent_data.cv.wait(guard, [&] { return !parent_data.queue.empty(); });
    resumable* job = parent_data.queue.front();
    parent_data.queue.pop_front();
    return job;
  }

  template <class Worker, class UnaryFunction>
  void foreach_resumable(Worker*, UnaryFunction) {
    // nop
  }

  template <class Coordinator, class UnaryFunction>
  void foreach_central_resumable(Coordinator* self, UnaryFunction f) {
    auto& queue = d(self).queue;
    auto next = [&]() -> resumable* {
      if (queue.empty()) {
        return nullptr;
      }
      auto front = queue.front();
      queue.pop_front();
      return front;
    };
    std::unique_lock<std::mutex> guard(d(self).lock);
    for (auto job = next(); job != nullptr; job = next()) {
      f(job);
    }
  }
};

} // namespace

// -- factory functions ------------------------------------------------------

std::unique_ptr<scheduler> scheduler::make_work_stealing(actor_system& sys) {
  return std::make_unique<scheduler_impl<work_stealing>>(sys);
}

std::unique_ptr<scheduler> scheduler::make_work_sharing(actor_system& sys) {
  return std::make_unique<scheduler_impl<work_sharing>>(sys);
}

// -- constructors -------------------------------------------------------------

scheduler::scheduler(actor_system& sys)
  : max_throughput_(0), num_workers_(0), system_(sys) {
  // nop
}

// -- implementation of actory_system::module ----------------------------------

void scheduler::start() {
  CAF_LOG_TRACE("");
  // Launch print utility actors.
  system_.printer(system_.spawn<printer_actor, hidden + detached>());
}

void scheduler::init(actor_system_config& cfg) {
  namespace sr = defaults::scheduler;
  max_throughput_ = get_or(cfg, "caf.scheduler.max-throughput",
                           sr::max_throughput);
  num_workers_ = get_or(cfg, "caf.scheduler.max-threads",
                        default_thread_count());
}

actor_system::module::id_t scheduler::id() const {
  return module::scheduler;
}

void* scheduler::subtype_ptr() {
  return this;
}

// -- utility functions --------------------------------------------------------

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
