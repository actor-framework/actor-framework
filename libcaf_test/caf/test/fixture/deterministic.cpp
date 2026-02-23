// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/test/fixture/deterministic.hpp"

#include "caf/test/reporter.hpp"

#include "caf/actor_control_block.hpp"
#include "caf/actor_registry.hpp"
#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/actor_system_module.hpp"
#include "caf/console_printer.hpp"
#include "caf/detail/actor_system_config_access.hpp"
#include "caf/detail/actor_system_impl.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/critical.hpp"
#include "caf/detail/daemons.hpp"
#include "caf/detail/mailbox_factory.hpp"
#include "caf/detail/meta_object.hpp"
#include "caf/detail/panic.hpp"
#include "caf/detail/print.hpp"
#include "caf/detail/private_thread_pool.hpp"
#include "caf/detail/sync_request_bouncer.hpp"
#include "caf/detail/test_export.hpp"
#include "caf/log/test.hpp"
#include "caf/logger.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/scheduled_actor.hpp"
#include "caf/scheduler.hpp"
#include "caf/telemetry/actor_metrics.hpp"
#include "caf/telemetry/metric_registry.hpp"
#include "caf/version.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <numeric>
#include <shared_mutex>
#include <source_location>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

using namespace std::literals;

namespace caf::test::fixture {

namespace {

using events_list_ptr = deterministic::events_list_ptr;

size_t mail_count(const deterministic::events_list& events) {
  size_t result = 0;
  for (auto& event : events)
    if (event->target && event->item)
      ++result;
  return result;
}

size_t mail_count(const deterministic::events_list& events,
                  scheduled_actor* receiver) {
  if (receiver == nullptr)
    return 0;
  auto pred = [&](const auto& event) { return event->target == receiver; };
  return std::ranges::count_if(events, pred);
}

size_t mail_count(const deterministic::events_list& events,
                  const strong_actor_ptr& receiver) {
  auto raw_ptr = actor_cast<abstract_actor*>(receiver);
  return mail_count(events, dynamic_cast<scheduled_actor*>(raw_ptr));
}

/// Removes the next message for `receiver` from the queue and returns it.
mailbox_element_ptr next_msg(deterministic::events_list& events,
                             scheduled_actor* receiver) {
  auto pred = [&](const auto& event) { return event->target == receiver; };
  auto first = events.begin();
  auto last = events.end();
  auto i = std::find_if(first, last, pred);
  if (i == last)
    return nullptr;
  auto result = std::move((*i)->item);
  events.erase(i);
  return result;
}

void drop_events(deterministic::events_list& events) {
  // Note: We cannot just call `events_.clear()`, because that would potentially
  //       cause an actor to become unreachable and close its mailbox. This
  //       could modify the events list in turn, which then tries to alter the
  //       list while we're clearing it.
  while (!events.empty()) {
    deterministic::events_list tmp;
    tmp.splice(tmp.end(), events);
  }
}

class deterministic_mailbox final : public ref_counted,
                                    public abstract_mailbox {
public:
  deterministic_mailbox(events_list_ptr events, scheduled_actor* owner)
    : events_(std::move(events)), owner_(owner) {
  }

  intrusive::inbox_result push_back(mailbox_element_ptr ptr) override {
    if (closed_) {
      detail::sync_request_bouncer bouncer;
      bouncer(*ptr);
      return intrusive::inbox_result::queue_closed;
    }
    using event_t = deterministic::scheduling_event;
    auto unblocked = mail_count(*events_, owner_) == 0;
    auto event = std::make_unique<event_t>(owner_, std::move(ptr));
    events_->push_back(std::move(event));
    return unblocked ? intrusive::inbox_result::unblocked_reader
                     : intrusive::inbox_result::success;
  }

  void push_front(mailbox_element_ptr ptr) override {
    using event_t = deterministic::scheduling_event;
    auto event = std::make_unique<event_t>(owner_, std::move(ptr));
    events_->emplace_front(std::move(event));
  }

  mailbox_element_ptr pop_front() override {
    return next_msg(*events_, owner_);
  }

  bool closed() const noexcept override {
    return closed_;
  }

  bool blocked() const noexcept override {
    return blocked_;
  }

  bool try_block() override {
    blocked_ = true;
    return true;
  }

  bool try_unblock() override {
    if (!blocked_)
      return false;
    blocked_ = false;
    return true;
  }

  size_t close(const error& reason) override {
    closed_ = true;
    close_reason_ = reason;
    auto result = size_t{0};
    auto bounce = detail::sync_request_bouncer{};
    auto envelope = next_msg(*events_, owner_);
    while (envelope != nullptr) {
      ++result;
      bounce(*envelope);
      envelope = next_msg(*events_, owner_);
    }
    return result;
  }

  size_t size() override {
    return mail_count(*events_, owner_);
  }

  void ref_mailbox() noexcept override {
    ref();
  }

  void deref_mailbox() noexcept override {
    deref();
  }

private:
  bool blocked_ = false;
  bool closed_ = false;
  error close_reason_;
  deterministic::events_list_ptr events_;
  scheduled_actor* owner_;
};

class deterministic_mailbox_factory final : public detail::mailbox_factory {
public:
  explicit deterministic_mailbox_factory(deterministic::events_list_ptr events)
    : events_(events) {
    // nop
  }

  abstract_mailbox* make(scheduled_actor* owner) override {
    return new deterministic_mailbox(events_, owner);
  }

  abstract_mailbox* make(blocking_actor*) override {
    return nullptr;
  }

private:
  deterministic::events_list_ptr events_;
};

class deterministic_registry final : public actor_registry {
public:
  void erase(actor_id key) override {
    auto i = entries_.find(key);
    if (i != entries_.end()) {
      entries_.erase(i);
    }
  }

  void erase(const std::string& key) override {
    auto i = named_entries_.find(key);
    if (i != named_entries_.end()) {
      named_entries_.erase(i);
    }
  }

  name_map named_actors() const override {
    return named_entries_;
  }

  void start() {
    // nop
  }

  void stop() {
    entries_.clear();
    named_entries_.clear();
  }

private:
  strong_actor_ptr get_impl(actor_id key) const override {
    auto i = entries_.find(key);
    if (i != entries_.end()) {
      return i->second;
    }
    return nullptr;
  }

  void put_impl(actor_id key, strong_actor_ptr val) override {
    if (!val || !entries_.emplace(key, val).second) {
      return;
    }
    val->get()->attach_functor([key, this]() { erase(key); });
  }

  strong_actor_ptr get_impl(const std::string& key) const override {
    if (auto i = named_entries_.find(key); i != named_entries_.end()) {
      return i->second;
    }
    return nullptr;
  }

  void put_impl(const std::string& key, strong_actor_ptr val) override {
    if (val == nullptr) {
      erase(key);
      return;
    }
    named_entries_.emplace(std::move(key), std::move(val));
  }

  std::unordered_map<actor_id, strong_actor_ptr> entries_;
  name_map named_entries_;
};

class deterministic_actor_clock final : public actor_clock {
public:
  // -- constructors, destructors, and assignment operators --------------------

  deterministic_actor_clock() : current_time(duration_type{1}) {
    // nop
  }

  // -- overrides --------------------------------------------------------------

  time_point now() const noexcept override {
    return current_time;
  }

  disposable schedule(time_point abs_time, action f) override {
    CAF_ASSERT(f.ptr() != nullptr);
    actions.emplace(abs_time, f);
    return std::move(f).as_disposable();
  }

  // -- testing DSL API --------------------------------------------------------

  /// Triggers the next pending timeout regardless of its timestamp. Sets
  /// `current_time` to the time point of the triggered timeout unless
  /// `current_time` is already set to a later time.
  /// @returns Whether a timeout was triggered.
  bool trigger_timeout(const std::source_location& loc) {
    drop_disposed();
    if (num_timeouts() == 0) {
      log::test::debug({"no pending timeout to trigger", loc});
      return false;
    }
    log::test::debug({"trigger next pending timeout", loc});
    if (auto delta = next_timeout(loc) - current_time; delta.count() > 0) {
      log::test::debug({"advance time by {}", loc}, duration_to_string(delta));
      current_time += delta;
    }
    if (!try_trigger_once()) {
      CAF_RAISE_ERROR("trigger_timeout failed to trigger a pending timeout");
    }
    return true;
  }

  /// Triggers all pending timeouts regardless of their timestamp. Sets
  /// `current_time` to the time point of the latest timeout unless
  /// `current_time` is already set to a later time.
  /// @returns The number of triggered timeouts.
  size_t trigger_all_timeouts(const std::source_location& loc) {
    drop_disposed();
    if (num_timeouts() == 0)
      return 0;
    if (auto t = last_timeout(loc); t > current_time)
      return advance_time(t - current_time, loc);
    auto result = size_t{0};
    while (try_trigger_once()) {
      ++result;
    }
    return result;
  }

  /// Advances the time by `x` and dispatches timeouts and delayed messages.
  /// @returns The number of triggered timeouts.
  size_t advance_time(duration_type x, const std::source_location& loc) {
    log::test::debug({"advance time by {}", loc}, duration_to_string(x));
    if (x.count() <= 0) {
      runnable::current().fail(
        {"advance_time requires a positive duration", loc});
    }
    current_time += x;
    auto result = size_t{0};
    drop_disposed();
    while (!actions.empty() && actions.begin()->first <= current_time) {
      if (try_trigger_once())
        ++result;
      drop_disposed(); // May have disposed timeouts.
    }
    return result;
  }

  /// Sets the current time.
  /// @returns The number of triggered timeouts.
  size_t set_time(actor_clock::time_point value,
                  const std::source_location& loc) {
    auto diff = value - current_time;
    if (diff.count() > 0)
      return advance_time(value - current_time, loc);
    auto msg = detail::format("set time back by {}", duration_to_string(diff));
    current_time = value;
    return 0;
  }

  void drop_actions() {
    for (auto [timeout, callback] : actions)
      callback.dispose();
    actions.clear();
  }

  // -- properties -------------------------------------------------------------

  /// Returns the number of pending timeouts.
  size_t num_timeouts() const noexcept {
    auto fn = [](size_t n, const auto& kvp) {
      return !kvp.second.disposed() ? n + 1 : n;
    };
    return std::accumulate(actions.begin(), actions.end(), size_t{0}, fn);
  }

  /// Returns the time of the next pending timeout.
  time_point next_timeout(const std::source_location& loc) {
    auto i = std::ranges::find_if(actions, is_not_disposed);
    if (i == actions.end())
      runnable::current().fail({"no pending timeout found", loc});
    return i->first;
  }

  /// Returns the time of the last pending timeout.
  time_point last_timeout(const std::source_location& loc) {
    auto i = std::find_if(actions.rbegin(), actions.rend(), is_not_disposed);
    if (i == actions.rend())
      runnable::current().fail({"no pending timeout found", loc});
    return i->first;
  }

  // -- member variables -------------------------------------------------------

  /// Stores the current time.
  time_point current_time;

  /// A map type for storing timeouts.
  using actions_map = std::multimap<time_point, action>;

  /// Stores the pending timeouts.
  actions_map actions;

private:
  /// Predicate that checks whether an action is not yet disposed.
  static bool is_not_disposed(const actions_map::value_type& x) noexcept {
    return !x.second.disposed();
  }

  /// Converts a duration to a string via `detail::print`.
  static std::string duration_to_string(actor_clock::duration_type x) {
    std::string result;
    detail::print(result, x);
    return result;
  }

  /// Removes all disposed actions from the actions map.
  void drop_disposed() {
    auto i = actions.begin();
    auto e = actions.end();
    while (i != e) {
      if (i->second.disposed())
        i = actions.erase(i);
      else
        ++i;
    }
  }

  /// Triggers the next timeout if it is due.
  bool try_trigger_once() {
    for (;;) {
      if (actions.empty())
        return false;
      auto i = actions.begin();
      auto [t, f] = *i;
      if (t > current_time)
        return false;
      actions.erase(i);
      if (!f.disposed()) {
        f.run();
        return true;
      }
    }
  }
};

class deterministic_scheduler final : public scheduler {
public:
  using super = caf::scheduler;

  explicit deterministic_scheduler(events_list_ptr events) : events_(events) {
    // nop
  }

  void schedule(resumable* ptr, uint64_t event_id) override {
    using event_t = deterministic::scheduling_event;
    // Actors put their messages into events_ directly when calling `push_back`
    // on the mailbox. We simply ignore the delay/schedule calls from actors
    // here except for initialization events (which we simply inline here).
    if (auto* self = dynamic_cast<scheduled_actor*>(ptr)) {
      if (event_id == resumable::initialization_event_id) {
        self->activate(this);
      }
    } else {
      // "Regular" resumables still need to be scheduled here.
      events_->push_back(std::make_unique<event_t>(ptr, nullptr));
    }
    // Before calling this function, CAF *always* bumps the reference count.
    // Hence, we need to release one reference count here.
    intrusive_ptr_release(ptr);
  }

  void delay(resumable* what, uint64_t event_id) override {
    schedule(what, event_id);
  }

  void start() override {
    // nop
  }

  void stop() override {
    drop_events(*events_);
  }

  bool is_system_scheduler() const noexcept override {
    return true;
  }

private:
  /// The events list that this scheduler is attached to.
  events_list_ptr events_;
};

class deterministic_actor_system final : public detail::actor_system_impl {
public:
  deterministic_actor_system(actor_system_config& cfg, events_list_ptr events)
    : cfg_(&cfg), events_(events), mailbox_factory_(events) {
    meta_objects_guard_ = detail::global_meta_objects_guard();
    if (!meta_objects_guard_)
      detail::critical("unable to obtain the global meta objects guard");
  }

  void start(actor_system& owner) override {
    detail::actor_system_config_access cfg_access{*cfg_};
    for (auto& hook : cfg_access.thread_hooks())
      hook->init(owner);
    logger_ = reporter::make_logger();
    CAF_SET_LOGGER_SYS(&owner);
    clock_ = std::make_unique<deterministic_actor_clock>();
    scheduler_ = std::make_unique<deterministic_scheduler>(events_);
    modules_[actor_system_module::daemons].reset(new detail::daemons(owner));
    for (auto& mod : modules_)
      if (mod)
        mod->init(*cfg_);
    registry_.start();
    private_threads_.start(owner);
    for (auto& mod : modules_)
      if (mod)
        mod->start();
    logger_->start();
  }

  void stop() override {
    clock_->drop_actions();
    drop_events(*events_);
    for (auto i = modules_.rbegin(); i != modules_.rend(); ++i) {
      if (*i)
        (*i)->stop();
    }
    if (scheduler_)
      scheduler_->stop();
    private_threads_.stop();
    registry_.stop();
    clock_ = nullptr;
    CAF_SET_LOGGER_SYS(nullptr);
    if (logger_) {
      logger_->stop();
      logger_ = nullptr;
    }
  }

  telemetry::actor_metrics make_actor_metrics(std::string_view) override {
    return {};
  }

  size_t inc_running_actors_count(actor_id) override {
    return ++running_actors_count_;
  }

  size_t dec_running_actors_count(actor_id) override {
    auto count = --running_actors_count_;
    if (count <= 1) {
      std::unique_lock guard{running_actors_mtx_};
      running_actors_cv_.notify_all();
    }
    return count;
  }

  void await_running_actors_count_equal(size_t expected,
                                        timespan timeout) override {
    CAF_ASSERT(expected == 0 || expected == 1);
    std::unique_lock guard{running_actors_mtx_};
    auto pred = [this, expected] {
      return running_actors_count_.load() == expected;
    };
    if (timeout == infinite)
      running_actors_cv_.wait(guard, pred);
    else
      running_actors_cv_.wait_for(guard, timeout, pred);
  }

  void thread_started(thread_owner) override {
    // nop
  }

  void thread_terminates() override {
    // nop
  }

  detail::global_meta_objects_guard_type
  meta_objects_guard() const noexcept override {
    return meta_objects_guard_;
  }

  actor_system_config& config() override {
    return *cfg_;
  }

  const actor_system_config& config() const override {
    return *cfg_;
  }

  actor_clock& clock() noexcept override {
    return *clock_;
  }

  size_t detached_actors() const noexcept override {
    return private_threads_.running();
  }

  bool await_actors_before_shutdown() const override {
    return await_actors_before_shutdown_;
  }

  void await_actors_before_shutdown(bool new_value) override {
    await_actors_before_shutdown_ = new_value;
  }

  telemetry::metric_registry& metrics() noexcept override {
    return metrics_;
  }

  const telemetry::metric_registry& metrics() const noexcept override {
    return metrics_;
  }

  const node_id& node() const override {
    return node_;
  }

  caf::scheduler& scheduler() override {
    return *scheduler_;
  }

  caf::logger& logger() override {
    return *logger_;
  }

  actor_registry& registry() override {
    return registry_;
  }

  std::span<std::unique_ptr<actor_system_module>> modules() override {
    return modules_;
  }

  actor_id next_actor_id() override {
    return ++ids_;
  }

  actor_id latest_actor_id() const override {
    return ids_.load();
  }

  size_t running_actors_count() const override {
    return running_actors_count_.load();
  }

  detail::private_thread* acquire_private_thread() override {
    return private_threads_.acquire();
  }

  void release_private_thread(detail::private_thread* ptr) override {
    private_threads_.release(ptr);
  }

  detail::mailbox_factory* mailbox_factory() override {
    return &mailbox_factory_;
  }

  void redirect_text_output(std::unique_ptr<console_printer> ptr) override {
    printer_ = std::move(ptr);
  }

  void do_print(term color, const char* buf, size_t num_bytes) override {
    if (printer_) {
      printer_->print(color, buf, num_bytes);
    } else {
      reporter::instance().println(log::level::info,
                                   std::string_view{buf, num_bytes});
    }
  }

  void set_node(node_id id) override {
    node_ = id;
  }

  void message_rejected(abstract_actor*) override {
    // nop for test impl
  }

  void launch(local_actor* ptr, caf::scheduler* ctx,
              spawn_options options) override {
    if (!has_hide_flag(options)) {
      ptr->setf(abstract_actor::is_registered_flag);
      inc_running_actors_count(ptr->id());
      // Note: decrementing the count happens in abstract_actor::cleanup().
    }
    // The detached flag is ignored in deterministic test mode. However,
    // blocking actors require detaching, so we need to abort the test if a user
    // attempts to spawn a blocking actor. The only exception is scoped actors,
    // which are blocking but not detached.
    if (has_detach_flag(options)
        && has_spawn_option(options, spawn_options::blocking_flag)) {
      detail::panic("blocking actors are not supported "
                    "in deterministic test mode");
    }
    // In the deterministic test mode, we never call launch and initialize
    // actors inline instead.
    ptr->launch_delayed();
    ptr->initialize(ctx);
  }

private:
  actor_system_config* cfg_;
  events_list_ptr events_;
  std::atomic<size_t> ids_{0};
  telemetry::metric_registry metrics_;
  node_id node_;
  deterministic_registry registry_;
  deterministic_mailbox_factory mailbox_factory_;
  std::atomic<size_t> running_actors_count_{0};
  mutable std::mutex running_actors_mtx_;
  mutable std::condition_variable running_actors_cv_;
  intrusive_ptr<detail::asynchronous_logger> logger_;
  std::unique_ptr<deterministic_actor_clock> clock_;
  std::unique_ptr<caf::scheduler> scheduler_;
  std::array<std::unique_ptr<actor_system_module>, actor_system_module::num_ids>
    modules_;
  bool await_actors_before_shutdown_ = true;
  detail::global_meta_objects_guard_type meta_objects_guard_;
  detail::private_thread_pool private_threads_;
  std::unique_ptr<console_printer> printer_;
};

} // namespace

// -- abstract_message_predicate -----------------------------------------------

deterministic::abstract_message_predicate::~abstract_message_predicate() {
  // nop
}

// -- constructors, destructors, and assignment operators ----------------------

namespace {

std::unique_ptr<detail::actor_system_impl>
make_deterministic_actor_system(actor_system_config& cfg,
                                events_list_ptr events) {
  cfg.set("caf.scheduler.max-throughput", 1);
  return std::make_unique<deterministic_actor_system>(cfg, events);
}

} // namespace

deterministic::deterministic()
  : deterministic(std::make_shared<events_list>()) {
  // nop
}

deterministic::deterministic(events_list_ptr events)
  : sys(make_deterministic_actor_system(cfg, events)), events_(events) {
  caf::test::runnable::current().current_metric_registry(&sys.metrics());
}

deterministic::~deterministic() {
  // Note: we need clean up all remaining messages manually. This in turn may
  //       clean up actors as unreachable if the test did not consume all
  //       messages. Otherwise, the destructor of `sys` will wait for all
  //       actors, potentially waiting forever. The same holds true for pending
  //       timeouts.
  dynamic_cast<deterministic_actor_clock&>(sys.clock()).drop_actions();
  drop_events(*events_);
}

// -- private utilities --------------------------------------------------------

bool deterministic::prepone_event_impl(const strong_actor_ptr& receiver) {
  actor_predicate any_sender{std::ignore};
  message_predicate any_payload{std::ignore};
  return prepone_event_impl(receiver, any_sender, any_payload);
}

bool deterministic::prepone_event_impl(
  const strong_actor_ptr& receiver, actor_predicate& sender_pred,
  abstract_message_predicate& payload_pred) {
  if (events_->empty() || !receiver)
    return false;
  auto first = events_->begin();
  auto last = events_->end();
  auto i = std::find_if(first, last, [&](const auto& event) {
    auto self = actor_cast<abstract_actor*>(receiver);
    return event->target == dynamic_cast<scheduled_actor*>(self)
           && sender_pred(event->item->sender)
           && payload_pred(event->item->payload);
  });
  if (i == last)
    return false;
  if (i != first) {
    auto ptr = std::move(*i);
    events_->erase(i);
    events_->insert(events_->begin(), std::move(ptr));
  }
  return true;
}

deterministic::scheduling_event*
deterministic::find_event_impl(const strong_actor_ptr& receiver) {
  if (events_->empty() || !receiver)
    return nullptr;
  auto last = events_->end();
  auto i = std::find_if(events_->begin(), last, [&](const auto& event) {
    auto raw_ptr = actor_cast<abstract_actor*>(receiver);
    return event->target == dynamic_cast<scheduled_actor*>(raw_ptr);
  });
  if (i != last)
    return i->get();
  return nullptr;
}

// -- properties ---------------------------------------------------------------

size_t deterministic::mail_count() {
  return fixture::mail_count(*events_);
}

size_t deterministic::mail_count(scheduled_actor* receiver) {
  return fixture::mail_count(*events_, receiver);
}

size_t deterministic::mail_count(const strong_actor_ptr& receiver) {
  return fixture::mail_count(*events_, receiver);
}

// -- control flow -------------------------------------------------------------

bool deterministic::terminated(const strong_actor_ptr& hdl) {
  auto base_ptr = actor_cast<abstract_actor*>(hdl);
  auto derived_ptr = dynamic_cast<scheduled_actor*>(base_ptr);
  if (derived_ptr == nullptr)
    CAF_RAISE_ERROR(std::invalid_argument,
                    "terminated: actor is not a scheduled actor");
  return derived_ptr->mailbox().closed();
}

bool deterministic::dispatch_message() {
  if (events_->empty())
    return false;
  if (events_->front()->item == nullptr) {
    // Regular resumable.
    auto ev = std::move(events_->front());
    events_->pop_front();
    auto hdl = ev->target;
    hdl->resume(&sys.scheduler(), resumable::default_event_id);
    return true;
  }
  // Actor: we simply resume the next actor and it will pick up its message.
  auto next = events_->front()->target;
  next->resume(&sys.scheduler(), resumable::default_event_id);
  return true;
}

size_t deterministic::dispatch_messages() {
  size_t result = 0;
  while (dispatch_message())
    ++result;
  return result;
}

void deterministic::inject_exit(const strong_actor_ptr& hdl, error reason) {
  if (!hdl)
    return;
  auto emsg = exit_msg{hdl->address(), std::move(reason)};
  if (!hdl->enqueue(make_mailbox_element(nullptr, make_message_id(), emsg),
                    nullptr)) {
    // Nothing to do here. The actor already terminated.
    return;
  }
  actor_predicate is_anon{nullptr};
  message_predicate<exit_msg> is_kill_msg{emsg};
  [[maybe_unused]] auto preponed = prepone_event_impl(hdl, is_anon,
                                                      is_kill_msg);
  assert(preponed);
  dispatch_message();
}

// -- time management ----------------------------------------------------------

size_t deterministic::set_time(actor_clock::time_point value,
                               const std::source_location& loc) {
  auto& clock = dynamic_cast<deterministic_actor_clock&>(sys.clock());
  return clock.set_time(value, loc);
}

size_t deterministic::advance_time(actor_clock::duration_type amount,
                                   const std::source_location& loc) {
  auto& clock = dynamic_cast<deterministic_actor_clock&>(sys.clock());
  return clock.advance_time(amount, loc);
}

bool deterministic::trigger_timeout(const std::source_location& loc) {
  auto& clock = dynamic_cast<deterministic_actor_clock&>(sys.clock());
  return clock.trigger_timeout(loc);
}

size_t deterministic::trigger_all_timeouts(const std::source_location& loc) {
  auto& clock = dynamic_cast<deterministic_actor_clock&>(sys.clock());
  return clock.trigger_all_timeouts(loc);
}

size_t deterministic::num_timeouts() noexcept {
  auto& clock = dynamic_cast<deterministic_actor_clock&>(sys.clock());
  return clock.num_timeouts();
}

actor_clock::time_point
deterministic::next_timeout(const std::source_location& loc) {
  auto& clock = dynamic_cast<deterministic_actor_clock&>(sys.clock());
  return clock.next_timeout(loc);
}

actor_clock::time_point
deterministic::last_timeout(const std::source_location& loc) {
  auto& clock = dynamic_cast<deterministic_actor_clock&>(sys.clock());
  return clock.last_timeout(loc);
}

} // namespace caf::test::fixture
