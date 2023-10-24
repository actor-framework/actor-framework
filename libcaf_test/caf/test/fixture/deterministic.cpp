// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/fixture/deterministic.hpp"

#include "caf/test/reporter.hpp"

#include "caf/actor_control_block.hpp"
#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/detail/mailbox_factory.hpp"
#include "caf/detail/print.hpp"
#include "caf/detail/source_location.hpp"
#include "caf/detail/sync_request_bouncer.hpp"
#include "caf/detail/test_export.hpp"
#include "caf/scheduled_actor.hpp"

#include <numeric>
#include <string>
#include <string_view>
#include <utility>

using namespace std::literals;
namespace {

using namespace caf;

/// A logger implementation for deterministic fixture to log into console
class deterministic_logger : public caf::logger,
                             public caf::detail::atomic_ref_counted {
public:
  /// Increases the reference count of the coordinated.
  void ref_logger() const noexcept final {
    this->ref();
  }

  /// Decreases the reference count of the coordinated and destroys the object
  /// if necessary.
  void deref_logger() const noexcept final {
    this->deref();
  }

  // -- constructors, destructors, and assignment operators --------------------

  deterministic_logger(actor_system&) {
    // nop
  }

  // -- logging ----------------------------------------------------------------

  /// Writes an entry to the event-queue of the logger.
  /// @thread-safe
  void do_log(const context& ctx, std::string&& msg) override {
    const auto location = caf::detail::source_location::current(
      ctx.file_name, ctx.function_name, ctx.line_number);
    switch (ctx.level) {
      case CAF_LOG_LEVEL_ERROR:
        reporter::instance().print_error(std::move(msg), location);
        return;
      case CAF_LOG_LEVEL_WARNING:
        reporter::instance().print_warning(std::move(msg), location);
        return;
      case CAF_LOG_LEVEL_INFO:
        reporter::instance().print_info(std::move(msg), location);
        return;
      case CAF_LOG_LEVEL_DEBUG:
        reporter::instance().print_debug(std::move(msg), location);
        return;
      case CAF_LOG_LEVEL_TRACE:
        return;
    }
  }

  /// Returns whether the logger is configured to accept input for given
  /// component and log level.
  bool accepts(unsigned level, std::string_view) override {
    return level <= reporter::instance().verbosity();
  }

  // -- initialization ---------------------------------------------------------

  /// Allows the logger to read its configuration from the actor system config.
  void init(const actor_system_config&) override {
    // nop
  }

  // -- event handling ---------------------------------------------------------

  /// Starts any background threads needed by the logger.
  void start() override {
    // nop
  }

  /// Stops all background threads of the logger.
  void stop() override {
    // nop
  }
};

} // namespace

namespace caf::test::fixture {

// -- mailbox ------------------------------------------------------------------

class deterministic::mailbox_impl : public ref_counted,
                                    public abstract_mailbox {
public:
  mailbox_impl(deterministic* fix, scheduled_actor* owner)
    : fix_(fix), owner_(owner) {
  }

  intrusive::inbox_result push_back(mailbox_element_ptr ptr) override {
    if (closed_) {
      detail::sync_request_bouncer bouncer{close_reason_};
      bouncer(*ptr);
      return intrusive::inbox_result::queue_closed;
    }
    using event_t = deterministic::scheduling_event;
    auto unblocked = fix_->mail_count(owner_) == 0;
    auto event = std::make_unique<event_t>(owner_, std::move(ptr));
    fix_->events_.push_back(std::move(event));
    return unblocked ? intrusive::inbox_result::unblocked_reader
                     : intrusive::inbox_result::success;
  }

  void push_front(mailbox_element_ptr ptr) override {
    using event_t = deterministic::scheduling_event;
    auto event = std::make_unique<event_t>(owner_, std::move(ptr));
    fix_->events_.emplace_front(std::move(event));
  }

  mailbox_element_ptr pop_front() override {
    return fix_->pop_msg_impl(owner_);
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
    auto bounce = detail::sync_request_bouncer{reason};
    auto envelope = fix_->pop_msg_impl(owner_);
    while (envelope != nullptr) {
      ++result;
      bounce(*envelope);
      envelope = fix_->pop_msg_impl(owner_);
    }
    return result;
  }

  size_t size() override {
    return fix_->mail_count(owner_);
  }

  void ref_mailbox() noexcept override {
    ref();
  }

  void deref_mailbox() noexcept override {
    deref();
  }

  mailbox_element* peek(message_id) override {
    // Note: this function only exists for backwards compatibility with the old
    // unit testing framework. It is not used by the new test runner and thus
    // not implemented.
    CAF_RAISE_ERROR(std::logic_error, "peek not supported by this mailbox");
  }

private:
  bool blocked_ = false;
  bool closed_ = false;
  error close_reason_;
  deterministic* fix_;
  scheduled_actor* owner_;
};

class deterministic::mailbox_factory_impl : public detail::mailbox_factory {
public:
  explicit mailbox_factory_impl(deterministic* fix) : fix_(fix) {
    // nop
  }

  abstract_mailbox* make(scheduled_actor* owner) override {
    return new deterministic::mailbox_impl(fix_, owner);
  }

  abstract_mailbox* make(blocking_actor*) override {
    return nullptr;
  }

private:
  deterministic* fix_;
};

// -- scheduler ----------------------------------------------------------------

namespace {

class actor_local_printer_impl : public detail::actor_local_printer {
public:
  explicit actor_local_printer_impl(local_actor* self) : self_(self) {
    // nop
  }

  void write(std::string&& arg) override {
    append(arg);
  }

  void write(const char* arg) override {
    append(std::string_view{arg});
  }

  void flush() override {
    auto str = std::string{line_.begin(), line_.end()};
    reporter::instance().print_actor_output(self_, str);
    line_.clear();
  }

private:
  void append(std::string_view str) {
    for (auto c : str) {
      if (c == '\n') {
        flush();
      } else {
        line_.push_back(c);
      }
    }
  }

  local_actor* self_;
  std::vector<char> line_;
};

class deterministic_actor_clock : public actor_clock {
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
  bool trigger_timeout(const detail::source_location& loc) {
    if (num_timeouts() == 0) {
      reporter::instance().print_debug("no pending timeout to trigger"sv, loc);
      return false;
    }
    reporter::instance().print_debug("trigger next pending timeout"sv, loc);
    if (auto delta = next_timeout(loc) - current_time; delta.count() > 0) {
      auto msg = detail::format("advance time by {}",
                                duration_to_string(delta));
      reporter::instance().print_debug(msg, loc);
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
  size_t trigger_all_timeouts(const detail::source_location& loc) {
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
  size_t advance_time(duration_type x, const detail::source_location& loc) {
    auto msg = detail::format("advance time by {}", duration_to_string(x));
    reporter::instance().print_debug(msg, loc);
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
                  const detail::source_location& loc) {
    auto diff = value - current_time;
    if (diff.count() > 0)
      return advance_time(value - current_time, loc);
    auto msg = detail::format("set time back by {}", duration_to_string(diff));
    current_time = value;
    return 0;
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
  time_point next_timeout(const detail::source_location& loc) {
    auto i = std::find_if(actions.begin(), actions.end(), is_not_disposed);
    if (i == actions.end())
      runnable::current().fail({"no pending timeout found", loc});
    return i->first;
  }

  /// Returns the time of the last pending timeout.
  time_point last_timeout(const detail::source_location& loc) {
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

} // namespace

class deterministic::scheduler_impl : public scheduler::abstract_coordinator {
public:
  using super = caf::scheduler::abstract_coordinator;

  scheduler_impl(actor_system& sys, deterministic* fix)
    : super(sys), fix_(fix) {
    // nop
  }

  detail::actor_local_printer_ptr printer_for(local_actor* self) override {
    auto& ptr = printers_[self->id()];
    if (!ptr)
      ptr = make_counted<actor_local_printer_impl>(self);
    return ptr;
  }

  bool detaches_utility_actors() const override {
    return false;
  }

  deterministic_actor_clock& clock() noexcept override {
    return clock_;
  }

protected:
  void start() override {
    // nop
  }

  void stop() override {
    fix_->drop_events();
  }

  void enqueue(resumable* ptr) override {
    using event_t = deterministic::scheduling_event;
    using subtype_t = resumable::subtype_t;
    switch (ptr->subtype()) {
      case subtype_t::scheduled_actor:
      case subtype_t::io_actor: {
        // Actors put their messages into events_ directly. However, we do run
        // them right away if they aren't initialized yet.
        auto dptr = static_cast<scheduled_actor*>(ptr);
        if (!dptr->initialized())
          dptr->resume(system_.dummy_execution_unit(), 0);
        break;
      }
      default:
        fix_->events_.push_back(std::make_unique<event_t>(ptr, nullptr));
    }
    // Before calling this function, CAF *always* bumps the reference count.
    // Hence, we need to release one reference count here.
    intrusive_ptr_release(ptr);
  }

private:
  /// The fixture this scheduler belongs to.
  deterministic* fix_;

  /// Allows users to fake time at will.
  deterministic_actor_clock clock_;

  /// Maps actors to their designated printer.
  std::map<actor_id, detail::actor_local_printer_ptr> printers_;
};

// -- config -------------------------------------------------------------------

deterministic::config::config(deterministic* fix) {
  factory_ = std::make_unique<mailbox_factory_impl>(fix);
  module_factories.push_back([fix](actor_system& sys) -> actor_system::module* {
    return new scheduler_impl(sys, fix);
  });
}

deterministic::config::~config() {
  // nop
}

detail::mailbox_factory* deterministic::config::mailbox_factory() {
  return factory_.get();
}

// -- abstract_message_predicate -----------------------------------------------

deterministic::abstract_message_predicate::~abstract_message_predicate() {
  // nop
}

// -- constructors, destructors, and assignment operators ----------------------

deterministic::deterministic()
  : cfg(this), sys(cfg.logger_factory([](caf::actor_system& sys) {
      return make_counted<deterministic_logger>(sys);
    })) {
  // nop
}

deterministic::~deterministic() {
  // Note: we need clean up all remaining messages manually. This in turn may
  //       clean up actors as unreachable if the test did not consume all
  //       messages. Otherwise, the destructor of `sys` will wait for all
  //       actors, potentially waiting forever.
  drop_events();
}

// -- private utilities --------------------------------------------------------

void deterministic::drop_events() {
  // Note: We cannot just call `events_.clear()`, because that would potentially
  //       cause an actor to become unreachable and close its mailbox. This
  //       could call `pop_msg_impl` in turn, which then tries to alter the list
  //       while we're clearing it.
  while (!events_.empty()) {
    std::list<std::unique_ptr<scheduling_event>> tmp;
    tmp.splice(tmp.end(), events_);
  }
}

bool deterministic::prepone_event_impl(const strong_actor_ptr& receiver) {
  actor_predicate any_sender{std::ignore};
  message_predicate any_payload{std::ignore};
  return prepone_event_impl(receiver, any_sender, any_payload);
}

bool deterministic::prepone_event_impl(
  const strong_actor_ptr& receiver, actor_predicate& sender_pred,
  abstract_message_predicate& payload_pred) {
  if (events_.empty() || !receiver)
    return false;
  auto first = events_.begin();
  auto last = events_.end();
  auto i = std::find_if(first, last, [&](const auto& event) {
    auto self = actor_cast<abstract_actor*>(receiver);
    return event->target == static_cast<scheduled_actor*>(self)
           && sender_pred(event->item->sender)
           && payload_pred(event->item->payload);
  });
  if (i == last)
    return false;
  if (i != first) {
    auto ptr = std::move(*i);
    events_.erase(i);
    events_.insert(events_.begin(), std::move(ptr));
  }
  return true;
}

deterministic::scheduling_event*
deterministic::find_event_impl(const strong_actor_ptr& receiver) {
  if (events_.empty() || !receiver)
    return nullptr;
  auto last = events_.end();
  auto i = std::find_if(events_.begin(), last, [&](const auto& event) {
    auto raw_ptr = actor_cast<abstract_actor*>(receiver);
    return event->target == static_cast<scheduled_actor*>(raw_ptr);
  });
  if (i != last)
    return i->get();
  return nullptr;
}

mailbox_element_ptr deterministic::pop_msg_impl(scheduled_actor* receiver) {
  auto pred = [&](const auto& event) { return event->target == receiver; };
  auto first = events_.begin();
  auto last = events_.end();
  auto i = std::find_if(first, last, pred);
  if (i == last)
    return nullptr;
  auto result = std::move((*i)->item);
  events_.erase(i);
  return result;
}

// -- properties ---------------------------------------------------------------

size_t deterministic::mail_count() {
  size_t result = 0;
  for (auto& event : events_)
    if (event->target)
      ++result;
  return result;
}

size_t deterministic::mail_count(scheduled_actor* receiver) {
  if (receiver == nullptr)
    return 0;
  auto pred = [&](const auto& event) { return event->target == receiver; };
  return std::count_if(events_.begin(), events_.end(), pred);
}

size_t deterministic::mail_count(const strong_actor_ptr& receiver) {
  auto raw_ptr = actor_cast<abstract_actor*>(receiver);
  return mail_count(dynamic_cast<scheduled_actor*>(raw_ptr));
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
  if (events_.empty())
    return false;
  if (events_.front()->item == nullptr) {
    // Regular resumable.
    auto ev = std::move(events_.front());
    events_.pop_front();
    auto hdl = ev->target;
    auto res = hdl->resume(sys.dummy_execution_unit(), 1);
    while (res == resumable::resume_later) {
      res = hdl->resume(sys.dummy_execution_unit(), 0);
    }
    return true;
  }
  // Actor: we simply resume the next actor and it will pick up its message.
  auto next = events_.front()->target;
  next->resume(sys.dummy_execution_unit(), 1);
  return true;
}

size_t deterministic::dispatch_messages() {
  size_t result = 0;
  while (dispatch_message())
    ++result;
  return result;
}

// -- time management ----------------------------------------------------------

size_t deterministic::set_time(actor_clock::time_point value,
                               const detail::source_location& loc) {
  return sched_impl().clock().set_time(value, loc);
}

size_t deterministic::advance_time(actor_clock::duration_type amount,
                                   const detail::source_location& loc) {
  return sched_impl().clock().advance_time(amount, loc);
}

bool deterministic::trigger_timeout(const detail::source_location& loc) {
  return sched_impl().clock().trigger_timeout(loc);
}

size_t deterministic::trigger_all_timeouts(const detail::source_location& loc) {
  return sched_impl().clock().trigger_all_timeouts(loc);
}

size_t deterministic::num_timeouts() noexcept {
  return sched_impl().clock().num_timeouts();
}

actor_clock::time_point
deterministic::next_timeout(const detail::source_location& loc) {
  return sched_impl().clock().next_timeout(loc);
}

actor_clock::time_point
deterministic::last_timeout(const detail::source_location& loc) {
  return sched_impl().clock().last_timeout(loc);
}

// -- private utilities --------------------------------------------------------

deterministic::scheduler_impl& deterministic::sched_impl() {
  return static_cast<scheduler_impl&>(sys.scheduler());
}

} // namespace caf::test::fixture
