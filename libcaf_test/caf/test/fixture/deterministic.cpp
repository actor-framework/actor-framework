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
#include "caf/detail/atomic_ref_count.hpp"
#include "caf/detail/critical.hpp"
#include "caf/detail/default_mailbox.hpp"
#include "caf/detail/mailbox_factory.hpp"
#include "caf/detail/meta_object.hpp"
#include "caf/detail/print.hpp"
#include "caf/detail/sync_request_bouncer.hpp"
#include "caf/fwd.hpp"
#include "caf/log/test.hpp"
#include "caf/logger.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/scheduler.hpp"
#include "caf/telemetry/actor_metrics.hpp"
#include "caf/telemetry/metric_registry.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <concepts>
#include <condition_variable>
#include <deque>
#include <iterator>
#include <memory>
#include <mutex>
#include <numeric>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>

using namespace std::literals;

namespace caf::test::fixture {

namespace {

template <class T>
concept receiver_arg
  = detail::one_of<std::remove_reference_t<T>, local_actor*, const local_actor*,
                   strong_actor_ptr, const strong_actor_ptr>;

/// A mailbox for storing all messages for all actors.
class global_mailbox {
public:
  struct entry {
    intrusive_ptr<local_actor> receiver;
    mailbox_element_ptr element;
  };

  /// Note: while a `vector` is usually faster, we use a linked list here
  /// because we are inserting at both ends and remove at arbitrary positions.
  using entry_list = std::list<entry>;

  /// Stores the entries of the global mailbox.
  entry_list entries;

  static auto by_receiver(const local_actor* receiver) {
    return [receiver](const auto& entry) { return entry.receiver == receiver; };
  }

  static auto by_receiver(const strong_actor_ptr& receiver) {
    return [ptr = receiver.get()](const auto& entry) {
      return entry.receiver->ctrl() == ptr;
    };
  }

  template <receiver_arg Receiver>
  auto find_entry(Receiver&& receiver) noexcept {
    return std::ranges::find_if(entries, by_receiver(receiver));
  }

  template <receiver_arg Receiver>
  size_t mail_count(Receiver&& receiver) const noexcept {
    return std::ranges::count_if(entries, by_receiver(receiver));
  }

  template <receiver_arg Receiver>
  mailbox_element* peek_next(Receiver&& receiver) noexcept {
    if (auto i = find_entry(receiver); i != entries.end()) {
      return i->element.get();
    }
    return nullptr;
  }

  template <receiver_arg Receiver>
  mailbox_element_ptr take_next(Receiver&& receiver) noexcept {
    if (auto i = find_entry(receiver); i != entries.end()) {
      auto result = std::move(i->element);
      entries.erase(i);
      return result;
    }
    return nullptr;
  }

  template <receiver_arg Receiver, class Predicate>
  mailbox_element_ptr
  take_next_if(Receiver&& receiver, Predicate&& pred) noexcept {
    auto p1 = by_receiver(receiver);
    auto p2 = [&p1, &pred](const auto& entry) {
      return p1(entry) && pred(entry.element);
    };
    if (auto i = std::ranges::find_if(entries, p2); i != entries.end()) {
      auto result = std::move(i->element);
      entries.erase(i);
      return result;
    }
    return nullptr;
  }

  template <receiver_arg Receiver, class ActorPredicate, class MessagePredicate>
  std::optional<entry> take_entry(Receiver&& receiver,
                                  ActorPredicate&& sender_pred,
                                  MessagePredicate&& payload_pred) {
    auto receiver_pred = by_receiver(receiver);
    auto pred = [&](const entry& entry) {
      return receiver_pred(entry) && sender_pred(entry.element->sender)
             && payload_pred(entry.element->payload);
    };
    if (auto i = std::ranges::find_if(entries, pred); i != entries.end()) {
      auto result = std::move(*i);
      entries.erase(i);
      return result;
    }
    return {};
  }

  template <std::invocable<const mailbox_element_ptr&> Visitor>
  size_t for_each(Visitor&& visitor) {
    auto result = size_t{0};
    for (const auto& entry : entries) {
      visitor(entry.element);
      ++result;
    }
    return result;
  }

  template <receiver_arg Receiver,
            std::invocable<const mailbox_element_ptr&> Visitor>
  size_t for_each(Receiver&& receiver, Visitor&& visitor) {
    auto pred = by_receiver(receiver);
    auto result = size_t{0};
    for (const auto& entry : entries) {
      if (pred(entry)) {
        visitor(entry.element);
        ++result;
      }
    }
    return result;
  }

  template <class Consumer>
  size_t consume_all(Consumer&& consumer) {
    auto result = size_t{0};
    auto i = entries.begin();
    while (i != entries.end()) {
      consumer(i->element);
      ++result;
      i = entries.erase(i);
    }
    return result;
  }

  template <receiver_arg Receiver, class Consumer>
  size_t consume_all(Receiver&& receiver, Consumer&& consumer) {
    auto result = size_t{0};
    auto pred = [receiver](const auto& entry) {
      return entry.receiver != receiver;
    };
    auto i = std::stable_partition(entries.begin(), entries.end(), pred);
    while (i != entries.end()) {
      consumer(i->element);
      ++result;
      i = entries.erase(i);
    }
    return result;
  }

  void take_all(std::vector<mailbox_element_ptr>& result) {
    consume_all([&result](auto& entry) { //
      result.emplace_back(std::move(entry));
    });
  }

  std::vector<mailbox_element_ptr> take_all() {
    auto result = std::vector<mailbox_element_ptr>{};
    take_all(result);
    return result;
  }

  /// Tries find a message that matches the given predicates and moves it to
  /// the front of the mailbox.
  template <receiver_arg Receiver, class ActorPredicate, class MessagePredicate>
  bool prepone(Receiver&& receiver, ActorPredicate&& sender_pred,
               MessagePredicate&& payload_pred) {
    if (entries.empty() || !receiver) {
      return false;
    }
    if (auto entry = take_entry(receiver, sender_pred, payload_pred)) {
      entries.emplace_front(std::move(*entry));
      return true;
    }
    return false;
  }
};

using global_mailbox_ptr = std::shared_ptr<global_mailbox>;

/// A central queue for storing all scheduled events for resumables.
class global_event_queue {
public:
  struct entry {
    intrusive_ptr<resumable> target;
    uint64_t event_id;
  };

  std::deque<entry> entries;
};

using global_event_queue_ptr = std::shared_ptr<global_event_queue>;

} // namespace

struct deterministic::private_data {
  global_mailbox_ptr messages = std::make_shared<global_mailbox>();
  global_event_queue_ptr events = std::make_shared<global_event_queue>();
};

namespace {

class deterministic_mailbox final : public abstract_mailbox {
public:
  deterministic_mailbox(global_mailbox_ptr global, local_actor* owner)
    : global_(std::move(global)), owner_(owner) {
    CAF_ASSERT(owner_ != nullptr);
    if (owner->getf(local_actor::is_blocking_flag)) {
      detail::critical("deterministic_mailbox cannot be used "
                       "with blocking actors");
    }
  }

  auto strong_owner_ptr() const noexcept {
    return intrusive_ptr<local_actor>{owner_, add_ref};
  }

  intrusive::inbox_result push_back(mailbox_element_ptr ptr) override {
    if (closed_) {
      detail::sync_request_bouncer bouncer;
      bouncer(*ptr);
      return intrusive::inbox_result::queue_closed;
    }
    global_->entries.emplace_back(strong_owner_ptr(), std::move(ptr));
    // Note: returning unblocked_reader would cause the actor to call
    //       `schedule`. Hence, we always return success here to make sure the
    //       actor never touches the scheduler.
    return intrusive::inbox_result::success;
  }

  void push_front(mailbox_element_ptr ptr) override {
    global_->entries.emplace_front(strong_owner_ptr(), std::move(ptr));
  }

  mailbox_element_ptr pop_front() override {
    return global_->take_next(owner_);
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

  bool close_if_blocked() override {
    closed_ = true;
    return true;
  }

  size_t close() override {
    closed_ = true;
    auto bounce = detail::sync_request_bouncer{};
    return global_->consume_all(owner_, bounce);
  }

  size_t size() override {
    return global_->mail_count(owner_);
  }

  void ref() const noexcept final {
    ref_count_.inc();
  }

  void deref() const noexcept final {
    ref_count_.dec(this);
  }

private:
  mutable detail::atomic_ref_count ref_count_;
  bool blocked_ = false;
  bool closed_ = false;
  global_mailbox_ptr global_;
  local_actor* owner_;
};

class deterministic_mailbox_factory final : public detail::mailbox_factory {
public:
  explicit deterministic_mailbox_factory(global_mailbox_ptr global)
    : global_(global) {
    // nop
  }

  abstract_mailbox* make(local_actor* owner) override {
    if (owner->is_scoped_actor_impl()) {
      // Scoped actors are the only supported blocking actor type in
      // deterministic test mode and they use the default mailbox.
      return new detail::default_mailbox;
    }
    if (owner->as_resumable() == nullptr) {
      detail::critical("deterministic_mailbox_factory: "
                       "actor does not implement the resumable interface");
    }
    return new deterministic_mailbox(global_, owner);
  }

private:
  global_mailbox_ptr global_;
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

  explicit deterministic_scheduler(global_event_queue_ptr global)
    : global_(std::move(global)) {
    // nop
  }

  void schedule(resumable_ptr ptr, uint64_t event_id) override {
    global_->entries.emplace_back(std::move(ptr), event_id);
  }

  void delay(resumable_ptr what, uint64_t event_id) override {
    schedule(std::move(what), event_id);
  }

  void start() override {
    // nop
  }

  void stop() override {
    // nop
  }

  bool is_system_scheduler() const noexcept override {
    return true;
  }

private:
  /// The events list that this scheduler is attached to.
  global_event_queue_ptr global_;
};

class deterministic_actor_system final : public detail::actor_system_impl {
public:
  deterministic_actor_system(actor_system_config& cfg,
                             deterministic::private_data* private_data)
    : cfg_(&cfg),
      private_data_(private_data),
      mailbox_factory_(private_data->messages) {
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
    scheduler_
      = std::make_unique<deterministic_scheduler>(private_data_->events);
    // Note: not loading the daemons module here on purpose because it is not
    //       supported in deterministic test mode.
    for (auto& mod : modules_)
      if (mod)
        mod->init(*cfg_);
    registry_.start();
    for (auto& mod : modules_)
      if (mod)
        mod->start();
    logger_->start();
  }

  void stop() override {
    clock_->drop_actions();
    private_data_->events->entries.clear();
    for (auto i = modules_.rbegin(); i != modules_.rend(); ++i) {
      if (*i)
        (*i)->stop();
    }
    if (scheduler_)
      scheduler_->stop();
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
    return 0;
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
    detail::critical("private threads are not supported "
                     "in deterministic test mode");
  }

  void release_private_thread(detail::private_thread*) override {
    detail::critical("private threads are not supported "
                     "in deterministic test mode");
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

  void max_throughput_reached(abstract_actor*) override {
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
      detail::critical("blocking actors are not supported "
                       "in deterministic test mode");
    }
    // In the deterministic test mode, we never call launch and initialize
    // actors inline instead.
    ptr->launch_delayed();
    ptr->initialize(ctx);
  }

private:
  actor_system_config* cfg_;
  deterministic::private_data* private_data_;
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
  std::unique_ptr<console_printer> printer_;
};

} // namespace

// -- nested classes -----------------------------------------------------------

deterministic::evaluator_base::~evaluator_base() noexcept {
  // nop
}

bool deterministic::evaluator_base::eval_dispatch(const strong_actor_ptr& dst,
                                                  bool fail_on_mismatch) {
  auto& ctx = runnable::current();
  // Check if the next message in the mailbox for the target actor
  // matches.
  auto* peek = fix_->private_data_->messages->peek_next(dst);
  if (!peek) {
    if (fail_on_mismatch)
      ctx.fail({"no message found for the receiver", loc_});
    return false;
  }
  if (!from_(peek->sender) || !with_(peek->payload)) {
    if (fail_on_mismatch)
      ctx.fail({"no matching message found, next message is: {}", loc_},
               peek->payload);
    return false;
  }
  if (priority_) {
    if (peek->mid.priority() != *priority_) {
      if (fail_on_mismatch)
        ctx.fail({"message priority does not match for message: {}", loc_},
                 peek->payload);
      return false;
    }
  }
  // This cast is safe, because we know that `dst` must be a local actor
  // at this point. Otherwise, peek would have failed to find a message.
  auto* self = actor_cast<local_actor*>(dst);
  if (self->is_terminated()) {
    ctx.fail({"actor has already terminated", loc_});
  }
  self->context(std::addressof(fix_->sys.scheduler()));
  // Remove the message from the mailbox and consume it.
  auto msg = fix_->private_data_->messages->take_next(dst);
  CAF_ASSERT(msg.get() == peek);
  if (self->consume(msg) == local_actor::consume_result::skipped) {
    ctx.fail({"actor skipped message: {}", loc_}, msg->payload);
  }
  reporter::instance().pass(loc_);
  return true;
}

bool deterministic::evaluator_base::dry_run(const strong_actor_ptr& dst) {
  if (auto* msg = fix_->private_data_->messages->peek_next(dst)) {
    return from_(msg->sender) && !with_(msg->payload);
  }
  return false;
}

bool deterministic::evaluator_base::eval_prepone(const strong_actor_ptr& dst) {
  return fix_->private_data_->messages->prepone(dst, from_, with_);
}

// -- constructors, destructors, and assignment operators ----------------------

namespace {

std::unique_ptr<detail::actor_system_impl>
make_deterministic_actor_system(actor_system_config& cfg,
                                deterministic::private_data* private_data) {
  cfg.set("caf.scheduler.max-throughput", 1);
  return std::make_unique<deterministic_actor_system>(cfg, private_data);
}

} // namespace

deterministic::deterministic()
  : private_data_(new private_data),
    sys(make_deterministic_actor_system(cfg, private_data_.get())) {
  caf::test::runnable::current().current_metric_registry(&sys.metrics());
}

deterministic::~deterministic() noexcept {
  // Note: we need clean up all remaining messages manually. This in turn may
  //       clean up actors as unreachable if the test did not consume all
  //       messages. Otherwise, the destructor of `sys` will wait for all
  //       actors, potentially waiting forever.
  static_cast<deterministic_actor_clock&>(sys.clock()).drop_actions();
  auto messages = private_data_->messages->take_all();
  while (!messages.empty()) {
    messages.clear(); // may trigger new messages such as exit_msg or down_msg
    static_cast<deterministic_actor_clock&>(sys.clock()).drop_actions();
    private_data_->messages->take_all(messages);
  }
}

// -- properties ---------------------------------------------------------------

size_t deterministic::pending_jobs() {
  return private_data_->events->entries.size();
}

size_t deterministic::mail_count() {
  return private_data_->messages->entries.size();
}

size_t deterministic::mail_count(local_actor* receiver) {
  return private_data_->messages->mail_count(receiver);
}

size_t deterministic::mail_count(const strong_actor_ptr& receiver) {
  return private_data_->messages->mail_count(receiver);
}

// -- control flow -------------------------------------------------------------

bool deterministic::terminated(const strong_actor_ptr& hdl) {
  if (!hdl) {
    CAF_RAISE_ERROR(std::invalid_argument, "terminated: handle is null");
  }
  auto* raw_ptr = actor_cast<abstract_actor*>(hdl);
  if (!raw_ptr->is_local_actor()) {
    CAF_RAISE_ERROR(std::invalid_argument,
                    "terminated: handle is not a local actor");
  }
  return raw_ptr->getf(abstract_actor::is_terminated_flag);
}

bool deterministic::dispatch_job(const std::source_location&) {
  auto& jobs = private_data_->events->entries;
  if (jobs.empty()) {
    return false;
  }
  auto event = std::move(jobs.front());
  jobs.pop_front();
  event.target->resume(std::addressof(sys.scheduler()), event.event_id);
  return true;
}

size_t deterministic::dispatch_jobs(const std::source_location&) {
  auto result = size_t{0};
  while (dispatch_job()) {
    ++result;
  }
  return result;
}

namespace {

template <class Filter>
bool dispatch_message_impl(deterministic& fix, global_mailbox& mbox,
                           Filter&& filter, const std::source_location& loc) {
  // Get all entries from the mailbox. Since processing a message may modify
  // the mailbox, we move the entries to a local list first.
  global_mailbox::entry_list entries;
  entries.swap(mbox.entries);
  // Utility for putting the entries back to the global mailbox.
  auto restore = [&mbox, &entries]() {
    mbox.entries.splice(mbox.entries.begin(), entries);
  };
  // Utility for dropping entries for a given receiver.
  auto drop_messages = [&entries](const intrusive_ptr<local_actor>& receiver) {
    auto bounce = detail::sync_request_bouncer{};
    auto pred = global_mailbox::by_receiver(receiver.get());
    for (auto i = entries.begin(); i != entries.end();) {
      if (!pred(*i)) {
        ++i;
        continue;
      }
      bounce(i->element);
      i = entries.erase(i);
    }
  };
  // Try to consume a single message.
  auto i = entries.begin();
  while (i != entries.end()) {
    if (!filter(*i)) {
      ++i;
      continue;
    }
    auto receiver = i->receiver;
    receiver->context(std::addressof(fix.sys.scheduler()));
    switch (receiver->consume(i->element)) {
      case local_actor::consume_result::consumed:
        entries.erase(i);
        restore();
        return true;
      case local_actor::consume_result::terminated:
        entries.erase(i);
        drop_messages(receiver);
        restore();
        return true;
      default:
        log::test::debug({"actor skipped message: {}", loc},
                         i->element->payload);
        ++i;
    }
  };
  restore();
  return false;
}

} // namespace

bool deterministic::dispatch_message(const std::source_location& loc) {
  auto pred = [](const auto&) { return true; };
  return dispatch_message_impl(*this, *private_data_->messages, pred, loc);
}

bool deterministic::dispatch_message(const strong_actor_ptr& hdl,
                                     const std::source_location& loc) {
  auto pred = [&hdl](const auto& entry) {
    return entry.receiver->ctrl() == hdl;
  };
  return dispatch_message_impl(*this, *private_data_->messages, pred, loc);
}

size_t deterministic::dispatch_messages(const std::source_location& loc) {
  size_t result = 0;
  while (dispatch_message(loc)) {
    ++result;
  }
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
  auto is_anon = rebindable_predicate<strong_actor_ptr>::from(nullptr);
  auto is_exit_msg = rebindable_predicate<const message&>{
    [emsg](const message& msg) { return msg.matches(emsg); }};
  [[maybe_unused]] auto ok = private_data_->messages->prepone(hdl, is_anon,
                                                              is_exit_msg);
  CAF_ASSERT(ok);
  dispatch_message();
}

// -- time management ----------------------------------------------------------

size_t deterministic::set_time(actor_clock::time_point value,
                               const std::source_location& loc) {
  auto& clock = static_cast<deterministic_actor_clock&>(sys.clock());
  return clock.set_time(value, loc);
}

size_t deterministic::advance_time(actor_clock::duration_type amount,
                                   const std::source_location& loc) {
  auto& clock = static_cast<deterministic_actor_clock&>(sys.clock());
  return clock.advance_time(amount, loc);
}

bool deterministic::trigger_timeout(const std::source_location& loc) {
  auto& clock = static_cast<deterministic_actor_clock&>(sys.clock());
  return clock.trigger_timeout(loc);
}

size_t deterministic::trigger_all_timeouts(const std::source_location& loc) {
  auto& clock = static_cast<deterministic_actor_clock&>(sys.clock());
  return clock.trigger_all_timeouts(loc);
}

size_t deterministic::num_timeouts() noexcept {
  const auto& clock = static_cast<deterministic_actor_clock&>(sys.clock());
  return clock.num_timeouts();
}

actor_clock::time_point
deterministic::next_timeout(const std::source_location& loc) {
  auto& clock = static_cast<deterministic_actor_clock&>(sys.clock());
  return clock.next_timeout(loc);
}

actor_clock::time_point
deterministic::last_timeout(const std::source_location& loc) {
  auto& clock = static_cast<deterministic_actor_clock&>(sys.clock());
  return clock.last_timeout(loc);
}

void deterministic::do_for_each_message(callback<void(const message&)>& fn) {
  auto dispatch = [&fn](const mailbox_element_ptr& element) {
    fn(element->payload);
  };
  private_data_->messages->for_each(dispatch);
}

void deterministic::do_for_each_message(const strong_actor_ptr& hdl,
                                        callback<void(const message&)>& fn) {
  auto dispatch = [&fn](const mailbox_element_ptr& element) {
    fn(element->payload);
  };
  private_data_->messages->for_each(hdl, dispatch);
}

} // namespace caf::test::fixture
