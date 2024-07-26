// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/abstract_mailbox.hpp"
#include "caf/abstract_scheduled_actor.hpp"
#include "caf/action.hpp"
#include "caf/actor_traits.hpp"
#include "caf/async/fwd.hpp"
#include "caf/config.hpp"
#include "caf/cow_string.hpp"
#include "caf/detail/behavior_stack.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/default_mailbox.hpp"
#include "caf/detail/stream_bridge.hpp"
#include "caf/disposable.hpp"
#include "caf/error.hpp"
#include "caf/flow/coordinator.hpp"
#include "caf/flow/fwd.hpp"
#include "caf/flow/multicaster.hpp"
#include "caf/fwd.hpp"
#include "caf/intrusive/stack.hpp"
#include "caf/invoke_message_result.hpp"
#include "caf/local_actor.hpp"
#include "caf/once.hpp"
#include "caf/ref.hpp"
#include "caf/repeat.hpp"
#include "caf/resumable.hpp"
#include "caf/telemetry/timer.hpp"
#include "caf/timespan.hpp"
#include "caf/unordered_flat_map.hpp"

#include <forward_list>
#include <type_traits>
#include <unordered_map>

#ifdef CAF_ENABLE_EXCEPTIONS
#  include <exception>
#  include <stdexcept>
#endif // CAF_ENABLE_EXCEPTIONS

namespace caf::detail {

class batch_forwarder_impl;

} // namespace caf::detail

namespace caf {

// -- related free functions ---------------------------------------------------

/// @relates scheduled_actor
/// Default handler function that sends the message back to the sender.
CAF_CORE_EXPORT skippable_result reflect(scheduled_actor*, message&);

/// @relates scheduled_actor
/// Default handler function that sends
/// the message back to the sender and then quits.
CAF_CORE_EXPORT skippable_result reflect_and_quit(scheduled_actor*, message&);

/// @relates scheduled_actor
/// Default handler function that prints a warning for the message to console
/// and drops it afterwards.
CAF_CORE_EXPORT skippable_result print_and_drop(scheduled_actor*, message&);

/// @relates scheduled_actor
/// Default handler function that simply drops messages.
CAF_CORE_EXPORT skippable_result drop(scheduled_actor*, message&);

/// A cooperatively scheduled, event-based actor implementation.
class CAF_CORE_EXPORT scheduled_actor : public abstract_scheduled_actor,
                                        public resumable,
                                        public flow::coordinator {
public:
  // -- friends ----------------------------------------------------------------

  friend class detail::batch_forwarder_impl;

  friend class detail::stream_bridge;

  friend class detail::stream_bridge_sub;

  template <class, class>
  friend class response_handle;

  // -- nested enums -----------------------------------------------------------

  /// Categorizes incoming messages.
  enum class message_category {
    /// Triggers the current behavior.
    ordinary,
    /// Triggers handlers for system messages such as `exit_msg` or `down_msg`.
    internal,
    /// Delays processing.
    skipped,
  };

  /// Result of one-shot activations.
  enum class activation_result {
    /// Actor is still alive and handled the activation message.
    success,
    /// Actor handled the activation message and terminated.
    terminated,
    /// Actor skipped the activation message.
    skipped,
    /// Actor dropped the activation message.
    dropped
  };

  // -- nested and member types ------------------------------------------------

  /// Base type.
  using super = abstract_scheduled_actor;

  using batch_op_ptr = intrusive_ptr<flow::op::base<async::batch>>;

  struct stream_source_state {
    batch_op_ptr obs;
    size_t max_items_per_batch;
  };

  /// The message ID of an outstanding response with its callback and timeout.
  using pending_response = std::tuple<const message_id, behavior, disposable>;

  /// A pointer to a scheduled actor.
  using pointer = scheduled_actor*;

  /// Function object for handling unmatched messages.
  using default_handler = std::function<skippable_result(pointer, message&)>;

  /// Function object for handling error messages.
  using error_handler = std::function<void(pointer, error&)>;

  /// Function object for handling down messages.
  using down_handler = std::function<void(pointer, down_msg&)>;

  /// Function object for handling node down messages.
  using node_down_handler = std::function<void(pointer, node_down_msg&)>;

  /// Function object for handling exit messages.
  using exit_handler = std::function<void(pointer, exit_msg&)>;

  /// Function object for handling timeouts.
  using idle_handler = std::function<void()>;

#ifdef CAF_ENABLE_EXCEPTIONS
  /// Function object for handling exit messages.
  using exception_handler = std::function<error(pointer, std::exception_ptr&)>;
#endif // CAF_ENABLE_EXCEPTIONS

  using batch_publisher = flow::multicaster<async::batch>;

  class batch_forwarder : public ref_counted {
  public:
    ~batch_forwarder() override;

    virtual void cancel() = 0;

    virtual void request(size_t num_items) = 0;
  };

  using batch_forwarder_ptr = intrusive_ptr<batch_forwarder>;

  // -- static helper functions ------------------------------------------------

  static void default_error_handler(pointer ptr, error& x);

  static void default_down_handler(pointer ptr, down_msg& x);

  static void default_node_down_handler(pointer ptr, node_down_msg& x);

  static void default_exit_handler(pointer ptr, exit_msg& x);

#ifdef CAF_ENABLE_EXCEPTIONS
  static error default_exception_handler(local_actor* ptr,
                                         std::exception_ptr& x);
#endif // CAF_ENABLE_EXCEPTIONS

  // -- constructors and destructors -------------------------------------------

  explicit scheduled_actor(actor_config& cfg);

  scheduled_actor(scheduled_actor&&) = delete;

  scheduled_actor(const scheduled_actor&) = delete;

  scheduled_actor& operator=(scheduled_actor&&) = delete;

  scheduled_actor& operator=(const scheduled_actor&) = delete;

  ~scheduled_actor() override;

  // -- overridden functions of abstract_actor ---------------------------------

  using abstract_actor::enqueue;

  bool enqueue(mailbox_element_ptr ptr, scheduler* sched) override;

  mailbox_element* peek_at_next_mailbox_element() override;

  // -- overridden functions of local_actor ------------------------------------

  const char* name() const override;

  void launch(scheduler* sched, bool lazy, bool hide) override;

  void on_cleanup(const error& reason) override;

  // -- overridden functions of resumable --------------------------------------

  subtype_t subtype() const noexcept override;

  void ref_resumable() const noexcept final;

  void deref_resumable() const noexcept final;

  resume_result resume(scheduler*, size_t) override;

  // -- scheduler callbacks ----------------------------------------------------

  /// Returns a factory for proxies created
  /// and managed by this actor or `nullptr`.
  virtual proxy_registry* proxy_registry_ptr();

  // -- state modifiers --------------------------------------------------------

  /// Finishes execution of this actor after any currently running message
  /// handler is done. This member function clears the behavior stack of the
  /// running actor and invokes `on_exit()`. The actors does not finish
  /// execution if the implementation of `on_exit()` sets a new behavior. When
  /// setting a new behavior in `on_exit()`, one has to make sure to not produce
  /// an infinite recursion.
  ///
  /// If `on_exit()` did not set a new behavior, the actor sends an exit message
  /// to all of its linked actors, sets its state to exited and finishes
  /// execution.
  void quit(error x = error{});

  // -- properties -------------------------------------------------------------

  /// Returns the queue for storing incoming messages.
  abstract_mailbox& mailbox() noexcept {
    return *mailbox_;
  }

  /// Checks whether this actor is fully initialized.
  bool initialized() const noexcept {
    return getf(is_initialized_flag);
  }

  /// Checks whether this actor is currently inactive, i.e., not ready to run.
  bool inactive() const noexcept {
    return getf(is_inactive_flag);
  }

  // -- event handlers ---------------------------------------------------------

  /// Sets a custom handler for unexpected messages.
  [[deprecated("use a handler for 'message' instead")]]
  void set_default_handler(default_handler fun) {
    if (fun)
      default_handler_ = std::move(fun);
    else
      default_handler_ = print_and_drop;
  }

  /// Sets a custom handler for unexpected messages.
  template <class F>
  [[deprecated("use a handler for 'message' instead")]]
  std::enable_if_t<std::is_invocable_r_v<skippable_result, F, message&>>
  set_default_handler(F fun) {
    default_handler_ = [fn{std::move(fun)}](scheduled_actor*,
                                            message& xs) mutable {
      return fn(xs);
    };
  }

  /// Sets a custom handler for error messages.
  [[deprecated("use a handler for 'error' instead")]]
  void set_error_handler(error_handler fun) {
    if (fun)
      error_handler_ = std::move(fun);
    else
      error_handler_ = default_error_handler;
  }

  /// Sets a custom handler for error messages.
  template <class F>
  [[deprecated("use a handler for 'error' instead")]]
  std::enable_if_t<std::is_invocable_v<F, error&>> set_error_handler(F fun) {
    error_handler_ = [fn{std::move(fun)}](scheduled_actor*, error& x) mutable {
      fn(x);
    };
  }

  /// Sets a custom handler for down messages.
  [[deprecated("use monitor with callback instead")]]
  void set_down_handler(down_handler fun) {
    if (fun)
      down_handler_ = std::move(fun);
    else
      down_handler_ = default_down_handler;
  }

  /// Sets a custom handler for down messages.
  template <class F>
  [[deprecated("use monitor with callback instead")]]
  std::enable_if_t<std::is_invocable_v<F, down_msg&>> set_down_handler(F fun) {
    down_handler_ = [fn{std::move(fun)}](scheduled_actor*,
                                         down_msg& x) mutable { fn(x); };
  }

  /// Sets a custom handler for node down messages.
  [[deprecated("use a handler for 'node_down_msg' instead")]]
  void set_node_down_handler(node_down_handler fun) {
    if (fun)
      node_down_handler_ = std::move(fun);
    else
      node_down_handler_ = default_node_down_handler;
  }

  /// Sets a custom handler for down messages.
  template <class F>
  [[deprecated("use a handler for 'node_down_msg' instead")]]
  std::enable_if_t<std::is_invocable_v<F, node_down_msg&>>
  set_node_down_handler(F fun) {
    node_down_handler_ = [fn{std::move(fun)}](scheduled_actor*,
                                              node_down_msg& x) mutable {
      fn(x);
    };
  }

  /// Sets a custom handler for error messages.
  [[deprecated("use a handler for 'exit_msg' instead")]]
  void set_exit_handler(exit_handler fun) {
    if (fun)
      exit_handler_ = std::move(fun);
    else
      exit_handler_ = default_exit_handler;
  }

  /// Sets a custom handler for exit messages.
  template <class F>
  [[deprecated("use a handler for 'exit_msg' instead")]]
  std::enable_if_t<std::is_invocable_v<F, exit_msg&>> set_exit_handler(F fun) {
    exit_handler_ = [fn{std::move(fun)}](scheduled_actor*,
                                         exit_msg& x) mutable { fn(x); };
  }

#ifdef CAF_ENABLE_EXCEPTIONS
  /// Sets a custom exception handler for this actor. If multiple handlers are
  /// defined, only the functor that was added *last* is being executed.
  void set_exception_handler(exception_handler fun) {
    if (fun)
      exception_handler_ = std::move(fun);
    else
      exception_handler_ = default_exception_handler;
  }

  /// Sets a custom exception handler for this actor. If multiple handlers are
  /// defined, only the functor that was added *last* is being executed.
  template <class F>
  std::enable_if_t<std::is_invocable_r_v<error, F, std::exception_ptr&>>
  set_exception_handler(F fun) {
    exception_handler_ = [fn{std::move(fun)}](scheduled_actor*,
                                              std::exception_ptr& x) mutable {
      return fn(x);
    };
  }
#endif // CAF_ENABLE_EXCEPTIONS

  /// Sets a custom handler for timeouts that trigger after *not* receiving
  /// a message for a certain amount of time.
  template <class RefType, class RepeatType>
  void set_idle_handler(timespan delay, RefType, RepeatType, idle_handler fun) {
    if (delay == infinite) {
#ifdef CAF_ENABLE_EXCEPTIONS
      throw std::invalid_argument(
        "cannot set an idle handler with infinite delay");
#else
      quit(make_error(sec::invalid_argument,
                      "cannot set an idle handler with infinite delay"));
      return;
#endif
    }
    timeout_state_.delay = delay;
    if constexpr (std::is_same_v<RefType, strong_ref_t>
                  && std::is_same_v<RepeatType, once_t>) {
      timeout_state_.mode = timeout_mode::once_strong;
    } else if constexpr (std::is_same_v<RefType, weak_ref_t>
                         && std::is_same_v<RepeatType, once_t>) {
      timeout_state_.mode = timeout_mode::once_weak;
    } else if constexpr (std::is_same_v<RefType, strong_ref_t>
                         && std::is_same_v<RepeatType, repeat_t>) {
      timeout_state_.mode = timeout_mode::repeat_strong;
    } else {
      static_assert(std::is_same_v<RefType, weak_ref_t>, "invalid RefType");
      static_assert(std::is_same_v<RepeatType, repeat_t>, "invalid RepeatType");
      timeout_state_.mode = timeout_mode::repeat_weak;
    }
    timeout_state_.handler = std::move(fun);
    set_receive_timeout();
  }

  /// @cond PRIVATE

  // -- timeout management -----------------------------------------------------

  /// Requests a new timeout for the current behavior.
  void set_receive_timeout();

  // -- message processing -----------------------------------------------------

  /// Adds a callback for an awaited response.
  void add_awaited_response_handler(message_id response_id, behavior bhvr,
                                    disposable pending_timeout = {}) override;

  /// Adds a callback for a multiplexed response.
  void
  add_multiplexed_response_handler(message_id response_id, behavior bhvr,
                                   disposable pending_timeout = {}) override;

  /// Returns the category of `x`.
  message_category categorize(mailbox_element& x);

  /// Tries to consume `x`.
  invoke_message_result consume(mailbox_element& x);

  /// Tries to consume `x`.
  void consume(mailbox_element_ptr x);

  /// Activates an actor and runs initialization code if necessary.
  /// @returns `true` if the actor is alive and ready for `reactivate`,
  ///          `false` otherwise.
  bool activate(scheduler* ctx);

  /// One-shot interface for activating an actor for a single message.
  activation_result activate(scheduler* ctx, mailbox_element& x);

  /// Interface for activating an actor any
  /// number of additional times after `activate`.
  activation_result reactivate(mailbox_element& x);

  // -- behavior management ----------------------------------------------------

  /// Returns `true` if the behavior stack is not empty.
  bool has_behavior() const noexcept {
    return !bhvr_stack_.empty();
  }

  /// Installs a new behavior without performing any type checks.
  void do_become(behavior bhvr, bool discard_old);

  /// Performs cleanup code for the actor if it has no active
  /// behavior or was explicitly terminated.
  /// @returns `true` if cleanup code was called, `false` otherwise.
  bool finalize();

  /// Returns the behavior stack.
  detail::behavior_stack& bhvr_stack() {
    return bhvr_stack_;
  }

  /// Pushes `ptr` to the cache of the default queue.
  void push_to_cache(mailbox_element_ptr ptr);

  // -- caf::flow API ----------------------------------------------------------

  steady_time_point steady_time() override;

  void ref_execution_context() const noexcept override;

  void deref_execution_context() const noexcept override;

  void schedule(action what) override;

  void delay(action what) override;

  void release_later(flow::coordinated_ptr& child) override;

  disposable delay_until(steady_time_point abs_time, action what) override;

  void watch(disposable what) override;

  /// Lifts a statically typed stream into an @ref observable.
  /// @param what The input stream.
  /// @param buf_capacity Upper bound for caching inputs from the stream.
  /// @param demand_threshold Minimal free buffer capacity before signaling
  ///                         demand upstream.
  /// @note Both @p buf_capacity and @p demand_threshold are considered hints.
  ///       The actor may increase (or decrease) the effective settings
  ///       depending on the amount of messages per batch or other factors.
  template <class T>
  flow::assert_scheduled_actor_hdr_t<flow::observable<T>>
  observe(typed_stream<T> what, size_t buf_capacity, size_t demand_threshold);

  /// Lifts a stream into an @ref observable.
  /// @param what The input stream.
  /// @param buf_capacity Upper bound for caching inputs from the stream.
  /// @param demand_threshold Minimal free buffer capacity before signaling
  ///                         demand upstream.
  /// @note Both @p buf_capacity and @p demand_threshold are considered hints.
  ///       The actor may increase (or decrease) the effective settings
  ///       depending on the amount of messages per batch or other factors.
  template <class T>
  flow::assert_scheduled_actor_hdr_t<flow::observable<T>>
  observe_as(stream what, size_t buf_capacity, size_t demand_threshold);

  /// Deregisters a local stream. After calling this function, other actors can
  /// no longer access the flow that has been attached to the stream. Current
  /// flows remain unaffected.
  void deregister_stream(uint64_t stream_id);

  /// @cond PRIVATE

  // -- utility functions for invoking default handler -------------------------

  /// Utility function that swaps `f` into a temporary before calling it
  /// and restoring `f` only if it has not been replaced by the user.
  template <class F, class... Ts>
  auto call_handler(F& f, Ts&&... xs)
    -> std::enable_if_t<
      !std::is_same_v<decltype(f(std::forward<Ts>(xs)...)), void>,
      decltype(f(std::forward<Ts>(xs)...))> {
    using std::swap;
    F g;
    swap(f, g);
    auto res = g(std::forward<Ts>(xs)...);
    if (!f)
      swap(g, f);
    return res;
  }

  template <class F, class... Ts>
  auto call_handler(F& f, Ts&&... xs)
    -> std::enable_if_t<
      std::is_same_v<decltype(f(std::forward<Ts>(xs)...)), void>> {
    using std::swap;
    F g;
    swap(f, g);
    g(std::forward<Ts>(xs)...);
    if (!f)
      swap(g, f);
  }

  void call_error_handler(error& err) override;

  // -- scheduling actions -----------------------------------------------------

  /// Runs `what` asynchronously at some point after `when`.
  /// @param when The local time until the actor waits before invoking the
  ///             action. Due to scheduling delays, there will always be some
  ///             additional wait time. Passing the current time or a past time
  ///             immediately schedules the action for execution.
  /// @param what The action to invoke after waiting on the timeout.
  /// @returns A @ref disposable that allows the actor to cancel the action.
  template <class Duration, class F>
  disposable run_scheduled(
    std::chrono::time_point<std::chrono::system_clock, Duration> when, F what) {
    using std::chrono::time_point_cast;
    return run_scheduled(time_point_cast<timespan>(when), make_action(what));
  }

  /// @copydoc run_scheduled
  template <class Duration, class F>
  disposable
  run_scheduled(std::chrono::time_point<actor_clock::clock_type, Duration> when,
                F what) {
    using std::chrono::time_point_cast;
    using duration_t = actor_clock::duration_type;
    return run_scheduled(time_point_cast<duration_t>(when), make_action(what));
  }

  /// Runs `what` asynchronously at some point after `when` if the actor still
  /// exists. The callback is going to hold a weak reference to the actor, i.e.,
  /// does not prevent the actor to become unreachable.
  /// @param when The local time until the actor waits before invoking the
  ///             action. Due to scheduling delays, there will always be some
  ///             additional wait time. Passing the current time or a past time
  ///             immediately schedules the action for execution.
  /// @param what The action to invoke after waiting on the timeout.
  /// @returns A @ref disposable that allows the actor to cancel the action.
  template <class Duration, class F>
  disposable run_scheduled_weak(
    std::chrono::time_point<std::chrono::system_clock, Duration> when, F what) {
    using std::chrono::time_point_cast;
    return run_scheduled_weak(time_point_cast<timespan>(when),
                              make_action(what));
  }

  /// @copydoc run_scheduled_weak
  template <class Duration, class F>
  disposable run_scheduled_weak(
    std::chrono::time_point<actor_clock::clock_type, Duration> when, F what) {
    using std::chrono::time_point_cast;
    using duration_t = actor_clock::duration_type;
    return run_scheduled_weak(time_point_cast<duration_t>(when),
                              make_action(what));
  }

  /// Runs `what` asynchronously after the `delay`.
  /// @param delay Minimum amount of time that actor waits before invoking the
  ///              action. Due to scheduling delays, there will always be some
  ///              additional wait time.
  /// @param what The action to invoke after the delay.
  /// @returns A @ref disposable that allows the actor to cancel the action.
  template <class Rep, class Period, class F>
  disposable run_delayed(std::chrono::duration<Rep, Period> delay, F what) {
    using std::chrono::duration_cast;
    return run_delayed(duration_cast<timespan>(delay), make_action(what));
  }

  /// Runs `what` asynchronously after the `delay` if the actor still exists.
  /// The callback is going to hold a weak reference to the actor, i.e., does
  /// not prevent the actor to become unreachable.
  /// @param delay Minimum amount of time that actor waits before invoking the
  ///              action. Due to scheduling delays, there will always be some
  ///              additional wait time.
  /// @param what The action to invoke after the delay.
  /// @returns A @ref disposable that allows the actor to cancel the action.
  template <class Rep, class Period, class F>
  disposable
  run_delayed_weak(std::chrono::duration<Rep, Period> delay, F what) {
    using std::chrono::duration_cast;
    return run_delayed_weak(duration_cast<timespan>(delay), make_action(what));
  }

  // -- monitoring -------------------------------------------------------------

  using super::monitor;

  using super::demonitor;

  template <message_priority P = message_priority::normal, class Handle>
  [[deprecated("use the monitor() overload with a callback instead")]]
  void monitor(const Handle& whom) {
    do_monitor(actor_cast<abstract_actor*>(whom), P);
  }

  template <class Handle>
  [[deprecated("use the monitor() overload with a callback instead")]]
  void demonitor(const Handle& whom) {
    do_demonitor(actor_cast<strong_actor_ptr>(whom));
  }

  /// Adds a unidirectional `monitor` to `whom` with custom callback.
  /// @returns a disposable object for canceling the monitoring of `whom`.
  /// @note This overload does not work with the @ref demonitor member function.
  template <class Handle, class Fn>
  disposable monitor(Handle whom, Fn func) {
    static_assert(!Handle::has_weak_ptr_semantics);
    static_assert(std::is_invocable_v<Fn, error>);
    using impl_t = detail::monitor_action<Fn>;
    return do_monitor(actor_cast<abstract_actor*>(whom),
                      make_counted<impl_t>(std::move(func)));
  }

  // -- properties -------------------------------------------------------------

  /// Returns `true` if the actor has a behavior, awaits responses, or
  /// participates in streams.
  /// @private
  bool alive() const noexcept {
    return !bhvr_stack_.empty() || !awaited_responses_.empty()
           || !multiplexed_responses_.empty() || !watched_disposables_.empty()
           || !stream_sources_.empty() || !stream_bridges_.empty();
  }

  /// Checks whether the actor is currently waiting for an external event, i.e.,
  /// a response message or external flow activity.
  /// @private
  [[nodiscard]] bool awaits_external_event() const noexcept {
    return !awaited_responses_.empty() || !multiplexed_responses_.empty()
           || !watched_disposables_.empty() || !stream_sources_.empty()
           || !stream_bridges_.empty();
  }

  /// Runs all pending actions.
  void run_actions() override;

  std::vector<disposable> watched_disposables() const {
    return watched_disposables_;
  }

  /// @endcond

protected:
  // -- member variables -------------------------------------------------------

  /// Stores incoming messages.
  abstract_mailbox* mailbox_;

  /// Stores user-defined callbacks for message handling.
  detail::behavior_stack bhvr_stack_;

  /// Stores callbacks for awaited responses.
  std::forward_list<pending_response> awaited_responses_;

  /// Stores callbacks for multiplexed responses.
  unordered_flat_map<message_id, std::pair<behavior, disposable>>
    multiplexed_responses_;

  /// Customization point for setting a default `message` callback.
  default_handler default_handler_;

  /// Customization point for setting a default `error` callback.
  error_handler error_handler_;

  /// Customization point for setting a default `down_msg` callback.
  down_handler down_handler_;

  /// Customization point for setting a default `down_msg` callback.
  node_down_handler node_down_handler_;

  /// Customization point for setting a default `exit_msg` callback.
  exit_handler exit_handler_;

  /// Pointer to a private thread object associated with a detached actor.
  detail::private_thread* private_thread_;

#ifdef CAF_ENABLE_EXCEPTIONS
  /// Customization point for setting a default exception callback.
  exception_handler exception_handler_;
#endif // CAF_ENABLE_EXCEPTIONS

private:
  using super::do_monitor;

  disposable do_monitor(abstract_actor* ptr,
                        detail::abstract_monitor_action_ptr on_down);

  /// Encodes how an actor is currently handling timeouts.
  enum class timeout_mode {
    none,          /// No timeout is set.
    once_weak,     /// The actor used the tags `once` and `weak`.
    once_strong,   /// The actor used the tags `once` and `strong`.
    repeat_weak,   /// The actor used the tags `repeat` and `weak`.
    repeat_strong, /// The actor used the tags `repeat` and `strong`.
    legacy,        /// The actor used a behavior with a timeout.
  };

  struct timeout_state {
    timeout_mode mode = timeout_mode::none;
    disposable pending;
    uint64_t id = 0;
    timespan delay = infinite;
    idle_handler handler;

    void reset() {
      mode = timeout_mode::none;
      pending = disposable{};
      id = 0;
      delay = infinite;
      handler = idle_handler{};
    }

    void swap(timeout_state& other) {
      using std::swap;
      swap(mode, other.mode);
      swap(pending, other.pending);
      swap(id, other.id);
      swap(delay, other.delay);
      swap(handler, other.handler);
    }
  };

  void handle_timeout();

  /// Stores the current timeout state.
  timeout_state timeout_state_;

  template <class T>
  flow::assert_scheduled_actor_hdr_t<flow::single<T>>
  single_from_response(message_id mid, disposable pending_timeout);

  void do_unstash(mailbox_element_ptr ptr) override;

  // -- utilities for instrumenting actors -------------------------------------

  /// Places all messages from the `stash_` back into the mailbox.
  void unstash();

  template <class F>
  activation_result run_with_metrics(mailbox_element& x, F body) {
    if (metrics_.mailbox_time) {
      auto t0 = std::chrono::steady_clock::now();
      auto mbox_time = x.seconds_until(t0);
      auto res = body();
      if (res != activation_result::skipped) {
        telemetry::timer::observe(metrics_.processing_time, t0);
        metrics_.mailbox_time->observe(mbox_time);
        metrics_.mailbox_size->dec();
      }
      return res;
    } else {
      return body();
    }
  }

  // -- cleanup ----------------------------------------------------------------

  void close_mailbox(const error& reason);

  void force_close_mailbox() final;

  // -- timeout management -----------------------------------------------------

  disposable run_scheduled(timestamp when, action what);
  disposable run_scheduled(actor_clock::time_point when, action what);
  disposable run_scheduled_weak(timestamp when, action what);
  disposable run_scheduled_weak(actor_clock::time_point when, action what);
  disposable run_delayed(timespan delay, action what);
  disposable run_delayed_weak(timespan delay, action what);

  // -- caf::flow bindings -----------------------------------------------------

  flow::coordinator* flow_context() override;

  template <class T, class Policy>
  flow::single<T> single_from_response(Policy& policy) {
    return single_from_response_impl<T>(policy);
  }

  template <class T, class Policy>
  flow::single<T> single_from_response_impl(Policy& policy);

  /// Removes any watched object that became disposed since the last update.
  void update_watched_disposables();

  /// Implementation detail for observe_as.
  flow::observable<async::batch> do_observe(stream what, size_t buf_capacity,
                                            size_t request_threshold);

  /// Implementation detail for to_stream.
  stream to_stream_impl(cow_string name, batch_op_ptr batch_op,
                        type_id_t item_type,
                        size_t max_items_per_batch) override;

  /// Registers a stream bridge at the actor (callback for
  /// detail::stream_bridge).
  void register_flow_state(uint64_t local_id,
                           detail::stream_bridge_sub_ptr sub);

  /// Drops the state for a stream bridge (callback for
  /// detail::stream_bridge_sub).
  void drop_flow_state(uint64_t local_id);

  /// Tries to emit more items on a stream bridge (callback for
  /// detail::stream_bridge_sub).
  void try_push_stream(uint64_t local_id);

  /// Cleans up any state associated to flows and streams and cancels all
  /// ongoing activities.
  void cancel_flows_and_streams();

  /// Stores actions that the actor executes after processing the current
  /// message.
  std::vector<action> actions_;

  /// Stores children that were marked for release while running an action.
  std::vector<flow::coordinated_ptr> released_;

  /// Counter for scheduled_actor::delay to make sure
  /// scheduled_actor::run_actions does not end up in a busy loop that might
  /// starve other activities.
  size_t delayed_actions_this_run_ = 0;

  /// Stores ongoing activities such as flows that block the actor from
  /// terminating.
  std::vector<disposable> watched_disposables_;

  /// Stores open streams that other actors may access. An actor is considered
  /// alive as long as it has open streams.
  std::unordered_map<uint64_t, stream_source_state> stream_sources_;

  /// Maps the ID of outgoing streams to local forwarder objects to allow the
  /// actor to signal demand from the receiver to the flow.
  std::unordered_map<uint64_t, batch_forwarder_ptr> stream_subs_;

  /// Maps the ID of incoming stream batches to local state that allows the
  /// actor to push received batches into the local flow.
  std::unordered_map<uint64_t, detail::stream_bridge_sub_ptr> stream_bridges_;

  /// Special-purpose behavior for scheduled_actor::delay. When pushing an
  /// action to the mailbox, we register this behavior as the response handler.
  /// This is to make sure that actor does not terminate because it thinks it's
  /// done before processing the delayed action.
  behavior delay_bhvr_;

  /// Stashes skipped messages until the actor processes the next message.
  intrusive::stack<mailbox_element> stash_;

  union {
    /// The default mailbox instance that we use if the user does not configure
    /// a mailbox via the ::actor_config.
    detail::default_mailbox default_mailbox_;
  };
};

} // namespace caf
