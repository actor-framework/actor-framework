// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/test/runnable.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/detail/source_location.hpp"
#include "caf/detail/test_actor_clock.hpp"
#include "caf/detail/test_export.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/scheduler/abstract_coordinator.hpp"

#include <list>
#include <memory>

namespace caf::test::fixture {

/// A fixture for writing unit tests that require deterministic scheduling. The
/// fixture equips tests with an actor system that uses a deterministic
/// scheduler and provides a DSL for writing high-level tests for message
/// passing between actors.
class CAF_TEST_EXPORT deterministic {
private:
  // -- private member types (implementation details) --------------------------

  class mailbox_impl;

  class scheduler_impl;

  class mailbox_factory_impl;

  /// Wraps a resumable pointer and a mailbox element pointer.
  struct scheduling_event {
    scheduling_event(resumable* target, mailbox_element_ptr payload)
      : target(target), item(std::move(payload)) {
      // nop
    }

    /// The target of the event.
    intrusive_ptr<resumable> target;

    /// The message for the event or `nullptr` if the target is not an actor.
    mailbox_element_ptr item;
  };

  /// A predicate for checking of single value. When constructing from a
  /// reference wrapper, the predicate assigns the found value to the reference
  /// instead of checking it.
  template <class T>
  class value_predicate {
  public:
    value_predicate() {
      predicate_ = [](const T&) { return true; };
    }

    explicit value_predicate(decltype(std::ignore)) : value_predicate() {
      // nop
    }

    template <class U>
    explicit value_predicate(
      U value, std::enable_if_t<detail::is_comparable_v<T, U>>* = nullptr) {
      predicate_ = [value](const T& found) { return found == value; };
    }

    explicit value_predicate(std::reference_wrapper<T> ref) {
      predicate_ = [ref](const T& found) {
        ref.get() = found;
        return true;
      };
    }

    template <
      class Predicate,
      class = std::enable_if_t<std::is_same_v<
        bool, decltype(std::declval<Predicate>()(std::declval<const T&>()))>>>
    explicit value_predicate(Predicate predicate) {
      predicate_ = std::move(predicate);
    }

    value_predicate(value_predicate&&) = default;

    value_predicate(const value_predicate&) = default;

    value_predicate& operator=(value_predicate&&) = default;

    value_predicate& operator=(const value_predicate&) = default;

    bool operator()(const T& value) {
      return predicate_(value);
    }

  private:
    std::function<bool(const T&)> predicate_;
  };

  /// Convenience alias for predicates on strong actor pointers.
  using actor_predicate = value_predicate<strong_actor_ptr>;

  /// Abstract base type for message predicates.
  class abstract_message_predicate {
  public:
    virtual ~abstract_message_predicate();

    virtual bool operator()(const message&) = 0;
  };

  /// A predicate for checking type and (optionally) content of a message.
  template <class... Ts>
  class message_predicate : public abstract_message_predicate {
  public:
    template <class U, class... Us>
    explicit message_predicate(U x, Us... xs) {
      predicates_ = std::make_shared<predicates_tuple>(std::move(x),
                                                       std::move(xs)...);
    }

    explicit message_predicate(decltype(std::ignore)) {
      // nop
    }

    message_predicate() {
      predicates_ = std::make_shared<predicates_tuple>();
    }

    message_predicate(message_predicate&&) = default;

    message_predicate(const message_predicate&) = default;

    message_predicate& operator=(message_predicate&&) = default;

    message_predicate& operator=(const message_predicate&) = default;

    bool operator()(const message& msg) {
      if (!predicates_)
        return true;
      if (auto view = make_const_typed_message_view<Ts...>(msg))
        return do_check(view, std::index_sequence_for<Ts...>{});
      return false;
    }

  private:
    template <size_t... Is>
    bool do_check([[maybe_unused]] const_typed_message_view<Ts...> view, //
                  std::index_sequence<Is...>) {
      return (((std::get<Is>(*predicates_))(get<Is>(view))) && ...);
    }

    using predicates_tuple = std::tuple<value_predicate<Ts>...>;

    std::shared_ptr<predicates_tuple> predicates_;
  };

public:
  // -- public member types ----------------------------------------------------

  /// The configuration type for this fixture.
  class config : public actor_system_config {
  public:
    config(deterministic* fix);

    ~config() override;

  private:
    detail::mailbox_factory* mailbox_factory() override;

    std::unique_ptr<detail::mailbox_factory> factory_;
  };

  /// Configures the algorithm to evaluate for an `evaluator` instances.
  enum class evaluator_algorithm {
    expect,
    allow,
    prepone,
    prepone_and_expect,
    prepone_and_allow
  };

  /// Provides a fluent interface for matching messages. The `evaluator` allows
  /// setting `from` and `with` parameters for an algorithm that matches
  /// messages against a predicate. When setting the only mandatory parameter
  /// `to`, the `evaluator` evaluates the predicate against the next message
  /// in the mailbox of the target actor.
  template <class... Ts>
  class evaluator {
  public:
    evaluator(deterministic* fix, detail::source_location loc,
              evaluator_algorithm algorithm)
      : fix_(fix), loc_(loc), algo_(algorithm) {
      // nop
    }

    evaluator() = delete;

    evaluator(evaluator&&) noexcept = default;

    evaluator& operator=(evaluator&&) noexcept = default;

    evaluator(const evaluator&) = delete;

    evaluator& operator=(const evaluator&) = delete;

    /// Matches the values of the message. The evaluator will match a message
    /// only if all individual values match the corresponding predicate.
    ///
    /// The template parameter pack `xs...` contains a list of match expressions
    /// that all must evaluate to true for a message to match. For each match
    /// expression:
    ///
    /// - Passing a value creates a predicate that matches the value exactly.
    /// - Passing a predicate (a function object taking one argument and
    ///   returning `bool`) will match any value for which the predicate returns
    ///   `true`.
    /// - Passing `std::ignore` accepts any value at that position.
    /// - Passing a `std::reference_wrapper<T>` will match any value and stores
    ///   the value in the reference wrapper.
    template <class... Us>
    evaluator&& with(Us... xs) && {
      static_assert(sizeof...(Ts) == sizeof...(Us));
      static_assert((std::is_constructible_v<value_predicate<Ts>, Us> && ...));
      with_ = message_predicate<Ts...>(std::move(xs)...);
      return std::move(*this);
    }

    /// Adds a predicate for the sender of the next message that matches only if
    /// the sender is `src`.
    evaluator&& from(const strong_actor_ptr& src) && {
      from_ = value_predicate<strong_actor_ptr>{src};
      return std::move(*this);
    }

    /// Adds a predicate for the sender of the next message that matches only if
    /// the sender is `src`.
    evaluator&& from(const actor& src) && {
      from_ = value_predicate<strong_actor_ptr>{src};
      return std::move(*this);
    }

    /// Adds a predicate for the sender of the next message that matches only if
    /// the sender is `src`.
    template <class... Us>
    evaluator&& from(const typed_actor<Us...>& src) && {
      from_ = value_predicate<strong_actor_ptr>{src};
      return std::move(*this);
    }

    /// Adds a predicate for the sender of the next message that matches only
    /// anonymous messages, i.e., messages without a sender.
    evaluator&& from(std::nullptr_t) && {
      return std::move(*this).from(strong_actor_ptr{});
    }

    /// Causes the evaluator to store the sender of a matched message in `src`.
    evaluator&& from(std::reference_wrapper<strong_actor_ptr> src) && {
      from_ = value_predicate<strong_actor_ptr>{src};
      return std::move(*this);
    }

    /// Sets the target actor for this evaluator and evaluate the predicate.
    template <class T>
    bool to(const T& dst) && {
      auto dst_ptr = actor_cast<strong_actor_ptr>(dst);
      switch (algo_) {
        case evaluator_algorithm::expect:
          return eval_dispatch(dst_ptr, true);
        case evaluator_algorithm::allow:
          return eval_dispatch(dst_ptr, false);
        case evaluator_algorithm::prepone:
          return eval_prepone(dst_ptr);
        case evaluator_algorithm::prepone_and_expect:
          eval_prepone(dst_ptr);
          return eval_dispatch(dst_ptr, true);
        case evaluator_algorithm::prepone_and_allow:
          return eval_prepone(dst_ptr) && eval_dispatch(dst_ptr, false);
        default:
          CAF_RAISE_ERROR(std::logic_error, "invalid algorithm");
      }
    }

  private:
    using predicates = std::tuple<value_predicate<Ts>...>;

    bool eval_dispatch(const strong_actor_ptr& dst, bool fail_on_mismatch) {
      auto& ctx = runnable::current();
      auto* event = fix_->find_event_impl(dst);
      if (!event) {
        if (fail_on_mismatch)
          ctx.fail({"no matching message found", loc_});
        return false;
      }
      if (!from_(event->item->sender) || !with_(event->item->payload)) {
        if (fail_on_mismatch)
          ctx.fail({"no matching message found", loc_});
        return false;
      }
      fix_->prepone_event_impl(dst);
      if (fail_on_mismatch) {
        if (!fix_->dispatch_message())
          ctx.fail({"failed to dispatch message", loc_});
        reporter::instance().pass(loc_);
        return true;
      }
      return fix_->dispatch_message();
    }

    bool eval_prepone(const strong_actor_ptr& dst) {
      return fix_->prepone_event_impl(dst, from_, with_);
    }

    deterministic* fix_;
    detail::source_location loc_;
    evaluator_algorithm algo_;
    actor_predicate from_;
    message_predicate<Ts...> with_;
  };

  /// Utility class for injecting messages into the mailbox of an actor and then
  /// checking whether the actor handles the message as expected.
  template <class... Ts>
  class injector {
  public:
    injector(deterministic* fix, detail::source_location loc, Ts... values)
      : fix_(fix), loc_(loc), values_(std::move(values)...) {
      // nop
    }

    injector() = delete;

    injector(injector&&) noexcept = default;

    injector& operator=(injector&&) noexcept = default;

    injector(const injector&) = delete;

    injector& operator=(const injector&) = delete;

    /// Adds a predicate for the sender of the next message that matches only if
    /// the sender is `src`.
    injector&& from(const strong_actor_ptr& src) && {
      from_ = src;
      return std::move(*this);
    }

    /// Adds a predicate for the sender of the next message that matches only if
    /// the sender is `src`.
    injector&& from(const actor& src) && {
      from_ = actor_cast<strong_actor_ptr>(src);
      return std::move(*this);
    }

    /// Adds a predicate for the sender of the next message that matches only if
    /// the sender is `src`.
    template <class... Us>
    injector&& from(const typed_actor<Us...>& src) && {
      from_ = actor_cast<strong_actor_ptr>(src);
      return std::move(*this);
    }

    /// Sets the target actor for this injector, sends the message, and then
    /// checks whether the actor handles the message as expected.
    template <class T>
    void to(const T& dst) && {
      to_impl(dst, std::make_index_sequence<sizeof...(Ts)>{});
    }

  private:
    template <class T, size_t... Is>
    void to_impl(const T& dst, std::index_sequence<Is...>) {
      auto ptr = actor_cast<abstract_actor*>(dst);
      ptr->eq_impl(make_message_id(), from_, nullptr,
                   make_message(std::get<Is>(values_)...));
      fix_->expect<Ts...>(loc_)
        .from(from_)
        .with(std::get<Is>(values_)...)
        .to(dst);
    }

    deterministic* fix_;
    detail::source_location loc_;
    strong_actor_ptr from_;
    std::tuple<Ts...> values_;
  };

  // -- friends ----------------------------------------------------------------

  friend class mailbox_impl;

  friend class scheduler_impl;

  template <class... Ts>
  friend class evaluator;

  // -- constructors, destructors, and assignment operators --------------------

  deterministic();

  ~deterministic();

  // -- properties -------------------------------------------------------------

  /// Returns the number of pending messages for `receiver`.
  size_t mail_count(scheduled_actor* receiver);

  /// Returns the number of pending messages for `receiver`.
  size_t mail_count(const strong_actor_ptr& receiver);

  /// Returns the number of pending messages for `receiver`.
  size_t mail_count(const actor& receiver) {
    return mail_count(actor_cast<strong_actor_ptr>(receiver));
  }

  /// Returns the number of pending messages for `receiver`.
  template <class... Ts>
  size_t mail_count(const typed_actor<Ts...>& receiver) {
    return mail_count(actor_cast<strong_actor_ptr>(receiver));
  }

  /// Checks whether `hdl` has terminated.
  bool terminated(const strong_actor_ptr& hdl);

  /// Checks whether `hdl` has terminated.
  template <class Handle>
  bool terminated(const Handle& hdl) {
    return terminated(actor_cast<strong_actor_ptr>(hdl));
  }

  // -- control flow -----------------------------------------------------------

  /// Tries to dispatch a single message.
  bool dispatch_message();

  /// Dispatches all pending messages.
  size_t dispatch_messages();

  // -- message-based predicates -----------------------------------------------

  /// Expects a message with types `Ts...` as the next message in the mailbox of
  /// the receiver and aborts the test if the message is missing. Otherwise
  /// executes the message.
  template <class... Ts>
  auto expect(const detail::source_location& loc
              = detail::source_location::current()) {
    return evaluator<Ts...>{this, loc, evaluator_algorithm::expect};
  }

  /// Tries to match a message with types `Ts...` and executes it if it is the
  /// next message in the mailbox of the receiver.
  template <class... Ts>
  auto allow(const detail::source_location& loc
             = detail::source_location::current()) {
    return evaluator<Ts...>{this, loc, evaluator_algorithm::allow};
  }

  /// Helper class for `inject` that only provides `with`.
  struct inject_helper {
    deterministic* fix;
    detail::source_location loc;

    template <class... Ts>
    auto with(Ts... values) {
      return injector<Ts...>{fix, loc, std::move(values)...};
    }
  };

  /// Starts an `inject` clause. Inject clauses provide a shortcut for sending a
  /// message to an actor and then calling `expect` with the same arguments to
  /// check whether the actor handles the message as expected.
  auto inject(const detail::source_location& loc
              = detail::source_location::current()) {
    return inject_helper{this, loc};
  }

  /// Tries to prepone a message, i.e., reorders the messages in the mailbox of
  /// the receiver such that the next call to `dispatch_message` will run it.
  /// @returns `true` if the message could be preponed, `false` otherwise.
  template <class... Ts>
  auto prepone(const detail::source_location& loc
               = detail::source_location::current()) {
    return evaluator<Ts...>{this, loc, evaluator_algorithm::prepone};
  }

  /// Shortcut for calling `prepone` and then `expect` with the same arguments.
  template <class... Ts>
  auto prepone_and_expect(const detail::source_location& loc
                          = detail::source_location::current()) {
    return evaluator<Ts...>{this, loc, evaluator_algorithm::prepone_and_expect};
  }

  /// Shortcut for calling `prepone` and then `allow` with the same arguments.
  template <class... Ts>
  auto prepone_and_allow(const detail::source_location& loc
                         = detail::source_location::current()) {
    return evaluator<Ts...>{this, loc, evaluator_algorithm::prepone_and_allow};
  }

  // -- member variables -------------------------------------------------------

private:
  // Note: this is put here because this member variable must be destroyed
  //       *after* the actor system (and thus must come before `sys` in
  //       the declaration order).

  /// Stores all pending messages of scheduled actors.
  std::list<std::unique_ptr<scheduling_event>> events_;

public:
  /// Configures the actor system with deterministic scheduling.
  config cfg;

  /// The actor system instance for the tests.
  actor_system sys;

private:
  /// Removes all events from the queue.
  void drop_events();

  /// Tries find a message in `events_` that matches the given predicate and
  /// moves it to the front of the queue.
  bool prepone_event_impl(const strong_actor_ptr& receiver);

  /// Tries find a message in `events_` that matches the given predicates and
  /// moves it to the front of the queue.
  bool prepone_event_impl(const strong_actor_ptr& receiver,
                          actor_predicate& sender_pred,
                          abstract_message_predicate& payload_pred);

  /// Returns the next event for `receiver` or `nullptr` if there is none.
  scheduling_event* find_event_impl(const strong_actor_ptr& receiver);

  /// Removes the next message for `receiver` from the queue and returns it.
  mailbox_element_ptr pop_msg_impl(scheduled_actor* receiver);
};

} // namespace caf::test::fixture
