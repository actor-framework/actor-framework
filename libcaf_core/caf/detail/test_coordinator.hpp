#include "caf/scheduled_actor.hpp"
#include "caf/scheduler.hpp"

#include <deque>

namespace caf::detail {

class test_actor_clock : public caf::actor_clock {
public:
  // -- constructors, destructors, and assignment operators ------------------

  test_actor_clock() : current_time(duration_type{1}) {
    // This ctor makes sure that the clock isn't at the default-constructed
    // time_point, because begin-of-epoch may have special meaning.
  }

  // -- overrides ------------------------------------------------------------

  time_point now() const noexcept override;

  caf::disposable schedule(time_point abs_time, caf::action f) override;

  // -- testing DSL API ------------------------------------------------------

  /// Returns whether the actor clock has at least one pending timeout.
  bool has_pending_timeout() const {
    auto not_disposed = [](const auto& kvp) { return !kvp.second.disposed(); };
    return std::any_of(actions.begin(), actions.end(), not_disposed);
  }

  /// Triggers the next pending timeout regardless of its timestamp. Sets
  /// `current_time` to the time point of the triggered timeout unless
  /// `current_time` is already set to a later time.
  /// @returns Whether a timeout was triggered.
  bool trigger_timeout();

  /// Triggers all pending timeouts regardless of their timestamp. Sets
  /// `current_time` to the time point of the latest timeout unless
  /// `current_time` is already set to a later time.
  /// @returns The number of triggered timeouts.
  size_t trigger_timeouts();

  /// Advances the time by `x` and dispatches timeouts and delayed messages.
  /// @returns The number of triggered timeouts.
  size_t advance_time(duration_type x);

  // -- properties -----------------------------------------------------------

  /// @pre has_pending_timeout()
  time_point next_timeout() const;

  // -- member variables -----------------------------------------------------

  time_point current_time;

  std::multimap<time_point, caf::action> actions;

private:
  bool try_trigger_once();
};

class test_coordinator : public scheduler {
public:
  using super = scheduler;

  /// A type-erased boolean predicate.
  using bool_predicate = std::function<bool()>;

  explicit test_coordinator(caf::actor_system& sys) : sys_(&sys) {
    // nop
  }

  caf::actor_system& system() {
    return *sys_;
  }

  /// A double-ended queue representing our current job queue.
  std::deque<caf::resumable*> jobs;

  /// Returns whether at least one job is in the queue.
  bool has_job() const {
    return !jobs.empty();
  }

  template <class T = caf::resumable>
  T& next_job() {
    if (jobs.empty())
      CAF_RAISE_ERROR("jobs.empty()");
    return dynamic_cast<T&>(*jobs.front());
  }

  template <class Handle>
  bool prioritize(const Handle& x) {
    auto ptr
      = dynamic_cast<caf::resumable*>(caf::actor_cast<caf::abstract_actor*>(x));
    return prioritize_impl(ptr);
  }

  /// Peeks into the mailbox of `next_job<scheduled_actor>()`.
  template <class... Ts>
  decltype(auto) peek() {
    auto ptr = next_job<caf::scheduled_actor>().peek_at_next_mailbox_element();
    CAF_ASSERT(ptr != nullptr);
    if (auto view = caf::make_const_typed_message_view<Ts...>(ptr->payload)) {
      if constexpr (sizeof...(Ts) == 1)
        return get<0>(view);
      else
        return to_tuple(view);
    } else {
      CAF_RAISE_ERROR("Mailbox element does not match.");
    }
  }

  /// Puts `x` at the front of the queue unless it cannot be found in the
  /// queue. Returns `true` if `x` exists in the queue and was put in front,
  /// `false` otherwise.
  bool prioritize_impl(caf::resumable* ptr);

  /// Runs all jobs that satisfy the predicate.
  template <class Predicate>
  size_t run_jobs_filtered(Predicate predicate) {
    size_t result = 0;
    while (!jobs.empty()) {
      auto i = std::find_if(jobs.begin(), jobs.end(), predicate);
      if (i == jobs.end())
        return result;
      if (i != jobs.begin())
        std::rotate(jobs.begin(), i, i + 1);
      run_once();
      ++result;
    }
    return result;
  }

  /// Tries to execute a single event in FIFO order.
  bool try_run_once();

  /// Tries to execute a single event in LIFO order.
  bool try_run_once_lifo();

  /// Executes a single event in FIFO order or fails if no event is available.
  void run_once();

  /// Executes a single event in LIFO order or fails if no event is available.
  void run_once_lifo();

  /// Executes events until the job queue is empty and no pending timeouts are
  /// left. Returns the number of processed events.
  size_t run(size_t max_count = std::numeric_limits<size_t>::max());

  /// Returns whether at least one pending timeout exists.
  bool has_pending_timeout();

  /// Tries to trigger a single timeout.
  bool trigger_timeout();

  /// Triggers all pending timeouts.
  size_t trigger_timeouts();

  /// Advances simulation time and returns the number of triggered timeouts.
  size_t advance_time(caf::timespan x);

  /// Call `f` after the next enqueue operation.
  template <class F>
  void after_next_enqueue(F f) {
    after_next_enqueue_ = f;
  }

  /// Executes the next enqueued job immediately by using the
  /// `after_next_enqueue` hook.
  void inline_next_enqueue();

  /// Executes all enqueued jobs immediately by using the `after_next_enqueue`
  /// hook.
  void inline_all_enqueues();

  /// Returns the test actor clock.
  test_actor_clock& clock();

protected:
  void start() override;

  void stop() override;

  void schedule(caf::resumable* ptr) override;

  void delay(caf::resumable* ptr) override;

private:
  void inline_all_enqueues_helper();

  caf::actor_system* sys_;

  /// User-provided callback for triggering custom code in `enqueue`.
  std::function<void()> after_next_enqueue_;
};

} // namespace caf::detail
