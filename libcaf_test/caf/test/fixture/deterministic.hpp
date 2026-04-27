// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/test/runnable.hpp"

#include "caf/actor_clock.hpp"
#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/config.hpp"
#include "caf/detail/concepts.hpp"
#include "caf/detail/default_actor_handle_codec.hpp"
#include "caf/detail/test_export.hpp"
#include "caf/local_actor.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/rebindable_predicate.hpp"

#include <memory>
#include <source_location>
#include <type_traits>

namespace caf::test::fixture {

/// A fixture for writing unit tests that require deterministic scheduling. The
/// fixture equips tests with an actor system that uses a deterministic
/// scheduler and provides a DSL for writing high-level tests for message
/// passing between actors.
class CAF_TEST_EXPORT deterministic {
public:
  /// Opaque data for the deterministic fixture.
  struct private_data;

  /// Configures the algorithm to evaluate for an `evaluator` instances.
  enum class evaluator_algorithm {
    expect,
    allow,
    disallow,
    prepone,
    prepone_and_expect,
    prepone_and_allow
  };

  class CAF_TEST_EXPORT evaluator_base {
  public:
    evaluator_base() = delete;

    evaluator_base(deterministic* fix, std::source_location loc,
                   evaluator_algorithm algorithm)
      : fix_(fix),
        loc_(loc),
        algo_(algorithm),
        from_([](const strong_actor_ptr&) { return true; }) {
      // nop
    }

    virtual ~evaluator_base() noexcept;

  protected:
    bool eval_dispatch(const strong_actor_ptr& dst, bool fail_on_mismatch);

    bool dry_run(const strong_actor_ptr& dst);

    bool eval_prepone(const strong_actor_ptr& dst);

    deterministic* fix_;
    std::source_location loc_;
    evaluator_algorithm algo_;
    rebindable_predicate<const strong_actor_ptr&> from_;
    std::optional<message_priority> priority_;
    rebindable_predicate<const message&> with_;
  };

  /// Provides a fluent interface for matching messages. The `evaluator`
  /// allows setting `from` and `with` parameters for an algorithm that
  /// matches messages against a predicate. When setting the only mandatory
  /// parameter `to`, the `evaluator` evaluates the predicate against the next
  /// message in the mailbox of the target actor.
  template <class... Ts>
  class evaluator : public evaluator_base {
  public:
    evaluator(deterministic* fix, std::source_location loc,
              evaluator_algorithm algorithm)
      : evaluator_base(fix, loc, algorithm) {
      this->with_ = [](const message& msg) {
        return msg.match_elements<Ts...>();
      };
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
    template <class... Args>
    evaluator&& with(Args... args) && {
      static_assert(sizeof...(Ts) == sizeof...(Args));
      auto nested
        = std::make_tuple(rebindable_predicate<Ts>::from(std::move(args))...);
      auto gn = [nested]<size_t... Is>(auto view, std::index_sequence<Is...>) {
        return (((std::get<Is>(nested))(get<Is>(view))) && ...);
      };
      auto fn = [gn](const message& msg) {
        if (auto view = make_const_typed_message_view<Ts...>(msg)) {
          return gn(view, std::make_index_sequence<sizeof...(Ts)>{});
        }
        return false;
      };
      this->with_ = std::move(fn);
      return std::move(*this);
    }

    /// Adds a predicate for the sender of the next message that matches only if
    /// the sender is `src`.
    evaluator&& from(const strong_actor_ptr& src) && {
      this->from_ = rebindable_predicate<strong_actor_ptr>::from(src);
      return std::move(*this);
    }

    /// Adds a predicate for the sender of the next message that matches only if
    /// the sender is `src`.
    evaluator&& from(const actor& src) && {
      this->from_ = rebindable_predicate<strong_actor_ptr>::from(src);
      return std::move(*this);
    }

    /// Adds a predicate for the sender of the next message that matches only if
    /// the sender is `src`.
    template <class... Us>
    evaluator&& from(const typed_actor<Us...>& src) && {
      this->from_ = rebindable_predicate<strong_actor_ptr>::from(src);
      return std::move(*this);
    }

    /// Adds a predicate for the sender of the next message that matches only
    /// anonymous messages, i.e., messages without a sender.
    evaluator&& from(std::nullptr_t) && {
      return std::move(*this).from(strong_actor_ptr{});
    }

    /// Causes the evaluator to store the sender of a matched message in `src`.
    evaluator&& from(std::reference_wrapper<strong_actor_ptr> src) && {
      this->from_ = rebindable_predicate<strong_actor_ptr>::from(src);
      return std::move(*this);
    }

    evaluator&& priority(message_priority priority) && {
      priority_ = priority;
      return std::move(*this);
    }

    /// Sets the target actor for this evaluator and evaluate the predicate.
    template <class T>
      requires(!std::is_same_v<T, scoped_actor>)
    bool to(const T& dst) && {
      auto dst_ptr = actor_cast<strong_actor_ptr>(dst);
      switch (this->algo_) {
        case evaluator_algorithm::expect:
          return eval_dispatch(dst_ptr, true);
        case evaluator_algorithm::allow:
          return eval_dispatch(dst_ptr, false);
        case evaluator_algorithm::disallow:
          if (dry_run(dst_ptr)) {
            auto& ctx = runnable::current();
            ctx.fail({"disallow message found", loc_});
          }
          return true;
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
  };

  /// Utility class for injecting messages into the mailbox of an actor and then
  /// checking whether the actor handles the message as expected.
  template <class... Ts>
  class injector {
  public:
    injector(deterministic* fix, std::source_location loc, Ts... values)
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
      ptr->enqueue(make_mailbox_element(from_, make_message_id(),
                                        std::get<Is>(values_)...),
                   nullptr);
      fix_->expect<Ts...>(loc_)
        .from(from_)
        .with(std::get<Is>(values_)...)
        .to(dst);
    }

    deterministic* fix_;
    std::source_location loc_;
    strong_actor_ptr from_;
    std::tuple<Ts...> values_;
  };

  /// Utility class for unconditionally killing an actor at scope exit.
  class actor_scope_guard {
  public:
    template <class Handle>
    actor_scope_guard(deterministic* fix, const Handle& dst) noexcept
      : fix_(fix) {
      if (dst)
        dst_ = actor_cast<strong_actor_ptr>(dst);
    }

    actor_scope_guard(actor_scope_guard&&) noexcept = default;

    actor_scope_guard() = delete;
    actor_scope_guard(const actor_scope_guard&) = delete;
    actor_scope_guard& operator=(const actor_scope_guard&) = delete;

    ~actor_scope_guard() {
      fix_->inject_exit(dst_, exit_reason::kill);
    }

  private:
    deterministic* fix_;
    strong_actor_ptr dst_;
  };

  // -- friends ----------------------------------------------------------------

  template <class... Ts>
  friend class evaluator;

  friend class actor_scope_guard;

  // -- constructors, destructors, and assignment operators --------------------

  deterministic();

  virtual ~deterministic() noexcept;

  // -- properties -------------------------------------------------------------

  /// Returns the number of pending jobs in the scheduler.
  size_t pending_jobs();

  /// Returns the number of pending messages in the system.
  size_t mail_count();

  /// Returns the number of pending messages for `receiver`.
  size_t mail_count(local_actor* receiver);

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

  /// Tries to dispatch a job in the scheduler.
  bool dispatch_job(const std::source_location& loc
                    = std::source_location::current());

  /// Dispatches all pending jobs in the scheduler.
  size_t dispatch_jobs(const std::source_location& loc
                       = std::source_location::current());

  /// Tries to dispatch a single message.
  bool dispatch_message(const std::source_location& loc
                        = std::source_location::current());

  /// Tries to dispatch a single message for `hdl`.
  bool dispatch_message(const strong_actor_ptr& hdl,
                        const std::source_location& loc
                        = std::source_location::current());

  /// Tries to dispatch a single message for `hdl`.
  bool dispatch_message(const actor& hdl, const std::source_location& loc
                                          = std::source_location::current()) {
    return dispatch_message(actor_cast<strong_actor_ptr>(hdl), loc);
  }

  /// Tries to dispatch a single message for `hdl`.
  template <class... Ts>
  bool dispatch_message(const typed_actor<Ts...>& hdl,
                        const std::source_location& loc
                        = std::source_location::current()) {
    return dispatch_message(actor_cast<strong_actor_ptr>(hdl), loc);
  }

  /// Dispatches all pending messages.
  size_t dispatch_messages(const std::source_location& loc
                           = std::source_location::current());

  // -- actor management -------------------------------------------------------

  /// Injects an exit message into the mailbox of `hdl` and dispatches it
  /// immediately.
  void inject_exit(const strong_actor_ptr& hdl,
                   error reason = exit_reason::user_shutdown);

  /// Injects an exit message into the mailbox of `hdl` and dispatches it
  /// immediately.
  template <class Handle>
  void
  inject_exit(const Handle& hdl, error reason = exit_reason::user_shutdown) {
    inject_exit(actor_cast<strong_actor_ptr>(hdl), std::move(reason));
  }

  // -- time management --------------------------------------------------------

  /// Sets the time to an arbitrary point in time.
  /// @returns the number of triggered timeouts.
  size_t
  set_time(actor_clock::time_point value,
           const std::source_location& loc = std::source_location::current());

  /// Advances the clock by `amount`.
  /// @returns the number of triggered timeouts.
  size_t advance_time(actor_clock::duration_type amount,
                      const std::source_location& loc
                      = std::source_location::current());

  /// Tries to trigger the next pending timeout. Returns `true` if a timeout
  /// was triggered, `false` otherwise.
  bool trigger_timeout(const std::source_location& loc
                       = std::source_location::current());

  /// Triggers all pending timeouts by advancing the clock to the point in time
  /// where the last timeout is due.
  size_t trigger_all_timeouts(const std::source_location& loc
                              = std::source_location::current());

  /// Returns the number of pending timeouts.
  [[nodiscard]] size_t num_timeouts() noexcept;

  /// Returns whether there is at least one pending timeout.
  [[nodiscard]] bool has_pending_timeout() noexcept {
    return num_timeouts() > 0;
  }

  /// Returns the time of the next pending timeout.
  [[nodiscard]] actor_clock::time_point
  next_timeout(const std::source_location& loc
               = std::source_location::current());

  /// Returns the time of the last pending timeout.
  [[nodiscard]] actor_clock::time_point
  last_timeout(const std::source_location& loc
               = std::source_location::current());

  // -- message-based predicates -----------------------------------------------

  /// Expects a message with types `Ts...` as the next message in the mailbox of
  /// the receiver and aborts the test if the message is missing. Otherwise
  /// executes the message.
  template <class... Ts>
  auto
  expect(const std::source_location& loc = std::source_location::current()) {
    return evaluator<Ts...>{this, loc, evaluator_algorithm::expect};
  }

  /// Tries to match a message with types `Ts...` and executes it if it is the
  /// next message in the mailbox of the receiver.
  template <class... Ts>
  auto
  allow(const std::source_location& loc = std::source_location::current()) {
    return evaluator<Ts...>{this, loc, evaluator_algorithm::allow};
  }

  /// Tries to match a message with types `Ts...` and executes it if it is the
  /// next message in the mailbox of the receiver.
  template <class... Ts>
  auto
  disallow(const std::source_location& loc = std::source_location::current()) {
    return evaluator<Ts...>{this, loc, evaluator_algorithm::disallow};
  }

  /// Helper class for `inject` that only provides `with`.
  struct inject_helper {
    deterministic* fix;
    std::source_location loc;

    template <class... Ts>
    auto with(Ts... values) {
      return injector<Ts...>{fix, loc, std::move(values)...};
    }
  };

  /// Starts an `inject` clause. Inject clauses provide a shortcut for sending a
  /// message to an actor and then calling `expect` with the same arguments to
  /// check whether the actor handles the message as expected.
  auto
  inject(const std::source_location& loc = std::source_location::current()) {
    return inject_helper{this, loc};
  }

  /// Tries to prepone a message, i.e., reorders the messages in the mailbox of
  /// the receiver such that the next call to `dispatch_message` will run it.
  /// @returns `true` if the message could be preponed, `false` otherwise.
  template <class... Ts>
  auto
  prepone(const std::source_location& loc = std::source_location::current()) {
    return evaluator<Ts...>{this, loc, evaluator_algorithm::prepone};
  }

  /// Shortcut for calling `prepone` and then `expect` with the same arguments.
  template <class... Ts>
  auto prepone_and_expect(const std::source_location& loc
                          = std::source_location::current()) {
    return evaluator<Ts...>{this, loc, evaluator_algorithm::prepone_and_expect};
  }

  /// Shortcut for calling `prepone` and then `allow` with the same arguments.
  template <class... Ts>
  auto prepone_and_allow(const std::source_location& loc
                         = std::source_location::current()) {
    return evaluator<Ts...>{this, loc, evaluator_algorithm::prepone_and_allow};
  }

  // -- serialization ----------------------------------------------------------

  template <class T>
  expected<T> serialization_roundtrip(const T& value) {
    byte_buffer buf;
    auto codec = detail::default_actor_handle_codec{sys};
    {
      binary_serializer sink{buf, &codec};
      if (!sink.apply(value))
        return unexpected<error>{std::in_place, sink.get_error()};
    }
    T result;
    {
      binary_deserializer source{buf, &codec};
      if (!source.apply(result))
        return unexpected<error>{std::in_place, source.get_error()};
    }
    return result;
  }

  // -- factories --------------------------------------------------------------

  /// Creates a new guard for `hdl` that will kill the actor at scope exit.
  template <class Handle>
  [[nodiscard]] actor_scope_guard make_actor_scope_guard(const Handle& hdl) {
    return {this, hdl};
  }

  /// Iterates over all pending messages.
  template <class Fn>
  void for_each_message(Fn&& fn) {
    using fn_t = std::remove_reference_t<Fn>;
    callback_ref_impl<fn_t, void(const message&)> fn_ref{fn};
    do_for_each_message(fn_ref);
  }

  /// Iterates over all pending messages in the mailbox of `hdl`.
  template <class Fn>
  void for_each_message(const strong_actor_ptr& hdl, Fn&& fn) {
    if (!hdl) {
      return;
    }
    using fn_t = std::remove_reference_t<Fn>;
    callback_ref_impl<fn_t, void(const message&)> fn_ref{fn};
    do_for_each_message(hdl, fn_ref);
  }

  // -- member variables -------------------------------------------------------

private:
  std::unique_ptr<private_data> private_data_;

public:
  /// The actor system configuration.
  actor_system_config cfg;

  /// The actor system instance for the tests.
  actor_system sys;

private:
  /// Applies `fn` to all pending messages.
  void do_for_each_message(callback<void(const message&)>& fn);

  /// Applies `fn` to all pending messages in the mailbox of `receiver`.
  void do_for_each_message(const strong_actor_ptr& receiver,
                           callback<void(const message&)>& fn);
};

} // namespace caf::test::fixture
