// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/config.hpp"

#ifdef CAF_ENABLE_EXCEPTIONS
#  include <exception>
#endif // CAF_ENABLE_EXCEPTIONS

#include <forward_list>
#include <map>
#include <type_traits>
#include <unordered_map>

#include "caf/action.hpp"
#include "caf/actor_traits.hpp"
#include "caf/async/fwd.hpp"
#include "caf/cow_string.hpp"
#include "caf/detail/behavior_stack.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/stream_bridge.hpp"
#include "caf/disposable.hpp"
#include "caf/error.hpp"
#include "caf/extend.hpp"
#include "caf/flow/coordinator.hpp"
#include "caf/flow/fwd.hpp"
#include "caf/flow/item_publisher.hpp"
#include "caf/flow/observer.hpp"
#include "caf/fwd.hpp"
#include "caf/intrusive/drr_cached_queue.hpp"
#include "caf/intrusive/drr_queue.hpp"
#include "caf/intrusive/fifo_inbox.hpp"
#include "caf/intrusive/wdrr_dynamic_multiplexed_queue.hpp"
#include "caf/intrusive/wdrr_fixed_multiplexed_queue.hpp"
#include "caf/invoke_message_result.hpp"
#include "caf/local_actor.hpp"
#include "caf/logger.hpp"
#include "caf/mixin/behavior_changer.hpp"
#include "caf/mixin/requester.hpp"
#include "caf/mixin/sender.hpp"
#include "caf/no_stages.hpp"
#include "caf/policy/arg.hpp"
#include "caf/policy/categorized.hpp"
#include "caf/policy/normal_messages.hpp"
#include "caf/policy/urgent_messages.hpp"
#include "caf/response_handle.hpp"
#include "caf/sec.hpp"
#include "caf/telemetry/timer.hpp"
#include "caf/unordered_flat_map.hpp"

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
/// Default handler function that prints messages
/// message via `aout` and drops them afterwards.
CAF_CORE_EXPORT skippable_result print_and_drop(scheduled_actor*, message&);

/// @relates scheduled_actor
/// Default handler function that simply drops messages.
CAF_CORE_EXPORT skippable_result drop(scheduled_actor*, message&);

/// A cooperatively scheduled, event-based actor implementation.
class CAF_CORE_EXPORT scheduled_actor : public local_actor,
                                        public resumable,
                                        public non_blocking_actor_base,
                                        public flow::coordinator {
public:
  // -- friends ----------------------------------------------------------------

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
  using super = local_actor;

  /// Stores asynchronous messages with default priority.
  using normal_queue = intrusive::drr_cached_queue<policy::normal_messages>;

  /// Stores asynchronous messages with hifh priority.
  using urgent_queue = intrusive::drr_cached_queue<policy::urgent_messages>;

  /// Configures the FIFO inbox with two nested queues. One for high-priority
  /// and one for normal priority messages.
  struct mailbox_policy {
    using deficit_type = size_t;

    using mapped_type = mailbox_element;

    using unique_pointer = mailbox_element_ptr;

    using queue_type
      = intrusive::wdrr_fixed_multiplexed_queue<policy::categorized,
                                                urgent_queue, normal_queue>;
  };

  using batch_op_ptr = intrusive_ptr<flow::op::base<async::batch>>;

  struct stream_source_state {
    batch_op_ptr obs;
    size_t max_items_per_batch;
  };

  static constexpr size_t urgent_queue_index = 0;

  static constexpr size_t normal_queue_index = 1;

  /// A queue optimized for single-reader-many-writers.
  using mailbox_type = intrusive::fifo_inbox<mailbox_policy>;

  /// The message ID of an outstanding response with its callback.
  using pending_response = std::pair<const message_id, behavior>;

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

#ifdef CAF_ENABLE_EXCEPTIONS
  /// Function object for handling exit messages.
  using exception_handler = std::function<error(pointer, std::exception_ptr&)>;
#endif // CAF_ENABLE_EXCEPTIONS

  using batch_publisher = flow::item_publisher<async::batch>;

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

  bool enqueue(mailbox_element_ptr ptr, execution_unit* eu) override;

  mailbox_element* peek_at_next_mailbox_element() override;

  // -- overridden functions of local_actor ------------------------------------

  const char* name() const override;

  void launch(execution_unit* eu, bool lazy, bool hide) override;

  bool cleanup(error&& fail_state, execution_unit* host) override;

  // -- overridden functions of resumable --------------------------------------

  subtype_t subtype() const override;

  void intrusive_ptr_add_ref_impl() override;

  void intrusive_ptr_release_impl() override;

  resume_result resume(execution_unit*, size_t) override;

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
  mailbox_type& mailbox() noexcept {
    return mailbox_;
  }

  // -- event handlers ---------------------------------------------------------

  /// Sets a custom handler for unexpected messages.
  void set_default_handler(default_handler fun) {
    if (fun)
      default_handler_ = std::move(fun);
    else
      default_handler_ = print_and_drop;
  }

  /// Sets a custom handler for unexpected messages.
  template <class F>
  std::enable_if_t<std::is_invocable_r_v<skippable_result, F, message&>>
  set_default_handler(F fun) {
    using std::move;
    default_handler_ = [fn{move(fun)}](scheduled_actor*, message& xs) mutable {
      return fn(xs);
    };
  }

  /// Sets a custom handler for error messages.
  void set_error_handler(error_handler fun) {
    if (fun)
      error_handler_ = std::move(fun);
    else
      error_handler_ = default_error_handler;
  }

  /// Sets a custom handler for error messages.
  template <class F>
  std::enable_if_t<std::is_invocable_v<F, error&>> set_error_handler(F fun) {
    error_handler_ = [fn{std::move(fun)}](scheduled_actor*, error& x) mutable {
      fn(x);
    };
  }

  /// Sets a custom handler for down messages.
  void set_down_handler(down_handler fun) {
    if (fun)
      down_handler_ = std::move(fun);
    else
      down_handler_ = default_down_handler;
  }

  /// Sets a custom handler for down messages.
  template <class F>
  std::enable_if_t<std::is_invocable_v<F, down_msg&>> set_down_handler(F fun) {
    using std::move;
    down_handler_ = [fn{move(fun)}](scheduled_actor*, down_msg& x) mutable {
      fn(x);
    };
  }

  /// Sets a custom handler for node down messages.
  void set_node_down_handler(node_down_handler fun) {
    if (fun)
      node_down_handler_ = std::move(fun);
    else
      node_down_handler_ = default_node_down_handler;
  }

  /// Sets a custom handler for down messages.
  template <class F>
  std::enable_if_t<std::is_invocable_v<F, node_down_msg&>>
  set_node_down_handler(F fun) {
    node_down_handler_ = [fn{std::move(fun)}](scheduled_actor*,
                                              node_down_msg& x) mutable {
      fn(x);
    };
  }

  /// Sets a custom handler for error messages.
  void set_exit_handler(exit_handler fun) {
    if (fun)
      exit_handler_ = std::move(fun);
    else
      exit_handler_ = default_exit_handler;
  }

  /// Sets a custom handler for exit messages.
  template <class F>
  std::enable_if_t<std::is_invocable_v<F, exit_msg&>> set_exit_handler(F fun) {
    using std::move;
    exit_handler_ = [fn{move(fun)}](scheduled_actor*, exit_msg& x) mutable {
      fn(x);
    };
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

  /// @cond PRIVATE

  // -- timeout management -----------------------------------------------------

  /// Requests a new timeout for the current behavior.
  void set_receive_timeout();

  // -- message processing -----------------------------------------------------

  /// Adds a callback for an awaited response.
  void add_awaited_response_handler(message_id response_id, behavior bhvr);

  /// Adds a callback for a multiplexed response.
  void add_multiplexed_response_handler(message_id response_id, behavior bhvr);

  /// Returns the category of `x`.
  message_category categorize(mailbox_element& x);

  /// Tries to consume `x`.
  invoke_message_result consume(mailbox_element& x);

  /// Tries to consume `x`.
  void consume(mailbox_element_ptr x);

  /// Activates an actor and runs initialization code if necessary.
  /// @returns `true` if the actor is alive and ready for `reactivate`,
  ///          `false` otherwise.
  bool activate(execution_unit* ctx);

  /// One-shot interface for activating an actor for a single message.
  activation_result activate(execution_unit* ctx, mailbox_element& x);

  /// Interface for activating an actor any
  /// number of additional times after `activate`.
  activation_result reactivate(mailbox_element& x);

  // -- behavior management ----------------------------------------------------

  /// Returns `true` if the behavior stack is not empty.
  bool has_behavior() const noexcept {
    return !bhvr_stack_.empty();
  }

  behavior& current_behavior() {
    return !awaited_responses_.empty() ? awaited_responses_.front().second
                                       : bhvr_stack_.back();
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

  /// Returns the queue of the mailbox that stores high priority messages.
  urgent_queue& get_urgent_queue();

  /// Returns the default queue of the mailbox that stores ordinary messages.
  normal_queue& get_normal_queue();

  // -- caf::flow API ----------------------------------------------------------

  steady_time_point steady_time() override;

  void ref_execution_context() const noexcept override;

  void deref_execution_context() const noexcept override;

  void schedule(action what) override;

  void delay(action what) override;

  disposable delay_until(steady_time_point abs_time, action what) override;

  void watch(disposable what) override;

  template <class Observable>
  flow::assert_scheduled_actor_hdr_t<Observable, stream>
  to_stream(std::string name, timespan max_delay, size_t max_items_per_batch,
            Observable&&);

  template <class Observable>
  flow::assert_scheduled_actor_hdr_t<Observable, stream>
  to_stream(cow_string name, timespan max_delay, size_t max_items_per_batch,
            Observable&&);

  struct to_stream_t {
    scheduled_actor* self;
    cow_string name;
    timespan max_delay;
    size_t max_items_per_batch;
    template <class Observable>
    auto operator()(Observable&& what) {
      return self->to_stream(name, max_delay, max_items_per_batch,
                             std::forward<Observable>(what));
    }
  };

  /// Returns a function object for passing it to @c compose.
  to_stream_t to_stream(std::string name, timespan max_delay,
                        size_t max_items_per_batch) {
    return {this, cow_string{std::move(name)}, max_delay, max_items_per_batch};
  }

  to_stream_t to_stream(cow_string name, timespan max_delay,
                        size_t max_items_per_batch) {
    return {this, std::move(name), max_delay, max_items_per_batch};
  }

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
  auto call_handler(F& f, Ts&&... xs) -> typename std::enable_if<
    !std::is_same<decltype(f(std::forward<Ts>(xs)...)), void>::value,
    decltype(f(std::forward<Ts>(xs)...))>::type {
    using std::swap;
    F g;
    swap(f, g);
    auto res = g(std::forward<Ts>(xs)...);
    if (!f)
      swap(g, f);
    return res;
  }

  template <class F, class... Ts>
  auto call_handler(F& f, Ts&&... xs) -> typename std::enable_if<
    std::is_same<decltype(f(std::forward<Ts>(xs)...)), void>::value>::type {
    using std::swap;
    F g;
    swap(f, g);
    g(std::forward<Ts>(xs)...);
    if (!f)
      swap(g, f);
  }

  void call_error_handler(error& err) {
    call_handler(error_handler_, this, err);
  }

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

  // -- properties -------------------------------------------------------------

  /// Returns `true` if the actor has a behavior, awaits responses, or
  /// participates in streams.
  /// @private
  bool alive() const noexcept {
    return !bhvr_stack_.empty() || !awaited_responses_.empty()
           || !multiplexed_responses_.empty() || !watched_disposables_.empty()
           || !stream_sources_.empty();
  }

  /// Runs all pending actions.
  void run_actions();

  std::vector<disposable> watched_disposables() const {
    return watched_disposables_;
  }

  /// @endcond

protected:
  // -- member variables -------------------------------------------------------

  /// Stores incoming messages.
  mailbox_type mailbox_;

  /// Stores user-defined callbacks for message handling.
  detail::behavior_stack bhvr_stack_;

  /// Allows us to cancel our current in-flight timeout.
  disposable pending_timeout_;

  /// Stores callbacks for awaited responses.
  std::forward_list<pending_response> awaited_responses_;

  /// Stores callbacks for multiplexed responses.
  unordered_flat_map<message_id, behavior> multiplexed_responses_;

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
  // -- utilities for instrumenting actors -------------------------------------

  template <class F>
  intrusive::task_result run_with_metrics(mailbox_element& x, F body) {
    if (metrics_.mailbox_time) {
      auto t0 = std::chrono::steady_clock::now();
      auto mbox_time = x.seconds_until(t0);
      auto res = body();
      if (res != intrusive::task_result::skip) {
        telemetry::timer::observe(metrics_.processing_time, t0);
        metrics_.mailbox_time->observe(mbox_time);
        metrics_.mailbox_size->dec();
      }
      return res;
    } else {
      return body();
    }
  }

  // -- timeout management -----------------------------------------------------

  disposable run_scheduled(timestamp when, action what);
  disposable run_scheduled(actor_clock::time_point when, action what);
  disposable run_delayed(timespan delay, action what);

  // -- caf::flow bindings -----------------------------------------------------

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
                        type_id_t item_type, size_t max_items_per_batch);

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

  /// Stores actions that the actor executes after processing the current
  /// message.
  std::vector<action> actions_;

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
};

} // namespace caf
