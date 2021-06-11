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

#include "caf/actor_traits.hpp"
#include "caf/detail/behavior_stack.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/unordered_flat_map.hpp"
#include "caf/error.hpp"
#include "caf/extend.hpp"
#include "caf/fwd.hpp"
#include "caf/inbound_path.hpp"
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
#include "caf/policy/downstream_messages.hpp"
#include "caf/policy/normal_messages.hpp"
#include "caf/policy/upstream_messages.hpp"
#include "caf/policy/urgent_messages.hpp"
#include "caf/response_handle.hpp"
#include "caf/sec.hpp"
#include "caf/stream_manager.hpp"
#include "caf/telemetry/timer.hpp"

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
                                        public non_blocking_actor_base {
public:
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

  /// Maps types to metric objects for inbound stream traffic.
  using inbound_stream_metrics_map
    = std::unordered_map<type_id_t, inbound_stream_metrics_t>;

  /// Maps types to metric objects for outbound stream traffic.
  using outbound_stream_metrics_map
    = std::unordered_map<type_id_t, outbound_stream_metrics_t>;

  /// Maps slot IDs to stream managers.
  using stream_manager_map = std::map<stream_slot, stream_manager_ptr>;

  /// Stores asynchronous messages with default priority.
  using normal_queue = intrusive::drr_cached_queue<policy::normal_messages>;

  /// Stores asynchronous messages with hifh priority.
  using urgent_queue = intrusive::drr_cached_queue<policy::urgent_messages>;

  /// Stores upstream messages.
  using upstream_queue = intrusive::drr_queue<policy::upstream_messages>;

  /// Stores downstream messages.
  using downstream_queue
    = intrusive::wdrr_dynamic_multiplexed_queue<policy::downstream_messages>;

  /// Configures the FIFO inbox with four nested queues:
  ///
  ///   1. Default asynchronous messages
  ///   2. High-priority asynchronous messages
  ///   3. Upstream messages
  ///   4. Downstream messages
  ///
  /// The queue for downstream messages is in turn composed of a nested queues,
  /// one for each active input slot.
  struct mailbox_policy {
    using deficit_type = size_t;

    using mapped_type = mailbox_element;

    using unique_pointer = mailbox_element_ptr;

    using queue_type = intrusive::wdrr_fixed_multiplexed_queue<
      policy::categorized, urgent_queue, normal_queue, upstream_queue,
      downstream_queue>;
  };

  static constexpr size_t urgent_queue_index = 0;

  static constexpr size_t normal_queue_index = 1;

  static constexpr size_t upstream_queue_index = 2;

  static constexpr size_t downstream_queue_index = 3;

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

  void enqueue(mailbox_element_ptr ptr, execution_unit* eu) override;

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

  /// Returns map for all active streams.
  stream_manager_map& stream_managers() noexcept {
    return stream_managers_;
  }

  /// Returns map for all pending streams.
  stream_manager_map& pending_stream_managers() noexcept {
    return pending_stream_managers_;
  }

  // -- actor metrics ----------------------------------------------------------

  inbound_stream_metrics_t inbound_stream_metrics(type_id_t type);

  outbound_stream_metrics_t outbound_stream_metrics(type_id_t type);

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

  /// Requests a new timeout and returns its ID.
  uint64_t set_receive_timeout(actor_clock::time_point x);

  /// Requests a new timeout for the current behavior and returns its ID.
  uint64_t set_receive_timeout();

  /// Resets the timeout if `timeout_id` is the active timeout.
  void reset_receive_timeout(uint64_t timeout_id);

  /// Returns whether `timeout_id` is currently active.
  bool is_active_receive_timeout(uint64_t tid) const;

  /// Requests a new timeout and returns its ID.
  uint64_t set_stream_timeout(actor_clock::time_point x);

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

  /// Returns the queue of the mailbox that stores `upstream_msg` messages.
  upstream_queue& get_upstream_queue();

  /// Returns the queue of the mailbox that stores `downstream_msg` messages.
  downstream_queue& get_downstream_queue();

  // -- inbound_path management ------------------------------------------------

  /// Creates a new path for incoming stream traffic from `sender`.
  virtual bool add_inbound_path(type_id_t input_type,
                                std::unique_ptr<inbound_path> path);

  /// Silently closes incoming stream traffic on `slot`.
  virtual void erase_inbound_path_later(stream_slot slot);

  /// Closes incoming stream traffic on `slot`. Emits a drop message on the
  /// path if `reason == none` and a `forced_drop` message otherwise.
  virtual void erase_inbound_path_later(stream_slot slot, error reason);

  /// Silently closes all inbound paths for `mgr`.
  virtual void erase_inbound_paths_later(const stream_manager* mgr);

  /// Closes all incoming stream traffic for a manager. Emits a drop message on
  /// each path if `reason == none` and a `forced_drop` message on each path
  /// otherwise.
  virtual void erase_inbound_paths_later(const stream_manager* mgr,
                                         error reason);

  // -- handling of stream messages --------------------------------------------

  void handle_upstream_msg(stream_slots slots, actor_addr& sender,
                           upstream_msg::ack_open& x);

  template <class T>
  void handle_upstream_msg(stream_slots slots, actor_addr& sender, T& x) {
    CAF_LOG_TRACE(CAF_ARG(slots) << CAF_ARG(sender) << CAF_ARG(x));
    CAF_IGNORE_UNUSED(sender);
    auto i = stream_managers_.find(slots.receiver);
    if (i == stream_managers_.end()) {
      auto j = pending_stream_managers_.find(slots.receiver);
      if (j != pending_stream_managers_.end()) {
        j->second->stop(sec::stream_init_failed);
        pending_stream_managers_.erase(j);
        return;
      }
      CAF_LOG_INFO("no manager found:" << CAF_ARG(slots));
      if constexpr (std::is_same<T, upstream_msg::ack_batch>::value) {
        // Make sure the other actor does not falsely believe us a source.
        inbound_path::emit_irregular_shutdown(this, slots, current_sender(),
                                              sec::invalid_upstream);
      }
      return;
    }
    CAF_ASSERT(i->second != nullptr);
    auto ptr = i->second;
    ptr->handle(slots, x);
    if (ptr->done()) {
      CAF_LOG_DEBUG("done sending:" << CAF_ARG(slots));
      ptr->stop();
      erase_stream_manager(ptr);
    } else if (ptr->out().path(slots.receiver) == nullptr) {
      CAF_LOG_DEBUG("done sending on path:" << CAF_ARG(slots.receiver));
      erase_stream_manager(slots.receiver);
    }
  }

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

  // -- timeout management -----------------------------------------------------

  /// Requests a new timeout and returns its ID.
  uint64_t set_timeout(std::string type, actor_clock::time_point x);

  // -- stream processing ------------------------------------------------------

  /// Returns a currently unused slot.
  stream_slot next_slot();

  /// Assigns slot `x` to `mgr`, i.e., adds a new entry to `stream_managers_`.
  void assign_slot(stream_slot x, stream_manager_ptr mgr);

  /// Assigns slot `x` to the pending manager `mgr`, i.e., adds a new entry to
  /// `pending_stream_managers_`.
  void assign_pending_slot(stream_slot x, stream_manager_ptr mgr);

  /// Convenience function for calling `assign_slot(next_slot(), mgr)`.
  stream_slot assign_next_slot_to(stream_manager_ptr mgr);

  /// Convenience function for calling `assign_slot(next_slot(), mgr)`.
  stream_slot assign_next_pending_slot_to(stream_manager_ptr mgr);

  /// Adds a new stream manager to the actor and starts cycle management if
  /// needed.
  bool add_stream_manager(stream_slot id, stream_manager_ptr ptr);

  /// Removes the stream manager mapped to `id` in `O(log n)`.
  void erase_stream_manager(stream_slot id);

  /// Removes the stream manager mapped to `id` in `O(log n)`.
  void erase_pending_stream_manager(stream_slot id);

  /// Moves a pending stream manager to the list of active stream managers.
  /// @returns `true` and a pointer to the moved stream manager on success,
  ///          `false` and `nullptr` otherwise.
  [[nodiscard]] std::pair<bool, stream_manager*>
  ack_pending_stream_manager(stream_slot id);

  /// Removes all entries for `mgr` in `O(n)`.
  void erase_stream_manager(const stream_manager_ptr& mgr);

  /// Processes a stream handshake.
  /// @pre `x.content().match_elements<open_stream_msg>()`
  invoke_message_result handle_open_stream_msg(mailbox_element& x);

  /// Advances credit and batch timeouts and returns the timestamp when to call
  /// this function again.
  actor_clock::time_point advance_streams(actor_clock::time_point now);

  // -- properties -------------------------------------------------------------

  /// Returns `true` if the actor has a behavior, awaits responses, or
  /// participates in streams.
  /// @private
  bool alive() const noexcept {
    return !bhvr_stack_.empty() || !awaited_responses_.empty()
           || !multiplexed_responses_.empty() || !stream_managers_.empty()
           || !pending_stream_managers_.empty();
  }

  auto max_batch_delay() const noexcept {
    return max_batch_delay_;
  }

  void active_stream_managers(std::vector<stream_manager*>& result);

  std::vector<stream_manager*> active_stream_managers();

  /// @endcond

protected:
  // -- member variables -------------------------------------------------------

  /// Stores incoming messages.
  mailbox_type mailbox_;

  /// Stores user-defined callbacks for message handling.
  detail::behavior_stack bhvr_stack_;

  /// Identifies the timeout messages we are currently waiting for.
  uint64_t timeout_id_;

  /// Stores callbacks for awaited responses.
  std::forward_list<pending_response> awaited_responses_;

  /// Stores callbacks for multiplexed responses.
  detail::unordered_flat_map<message_id, behavior> multiplexed_responses_;

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

  /// Stores stream managers for established streams.
  stream_manager_map stream_managers_;

  /// Stores stream managers for pending streams, i.e., streams that have not
  /// yet received an ACK.
  stream_manager_map pending_stream_managers_;

  /// Stores how long the actor should try to accumulate more items in order to
  /// send a full stream batch.
  timespan max_batch_delay_;

  /// Number of ticks per batch delay.
  size_t max_batch_delay_ticks_;

  /// Pointer to a private thread object associated with a detached actor.
  detail::private_thread* private_thread_;

  /// Caches metric objects for inbound stream traffic.
  inbound_stream_metrics_map inbound_stream_metrics_;

  /// Caches metric objects for outbound stream traffic.
  outbound_stream_metrics_map outbound_stream_metrics_;

#ifdef CAF_ENABLE_EXCEPTIONS
  /// Customization point for setting a default exception callback.
  exception_handler exception_handler_;
#endif // CAF_ENABLE_EXCEPTIONS

private:
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
};

} // namespace caf
