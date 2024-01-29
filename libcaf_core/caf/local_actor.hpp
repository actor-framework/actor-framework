// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/abstract_actor.hpp"
#include "caf/actor.hpp"
#include "caf/actor_cast.hpp"
#include "caf/actor_config.hpp"
#include "caf/actor_system.hpp"
#include "caf/behavior.hpp"
#include "caf/delegated.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/send_type_check.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/detail/typed_actor_util.hpp"
#include "caf/detail/unique_function.hpp"
#include "caf/disposable.hpp"
#include "caf/error.hpp"
#include "caf/fwd.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/message.hpp"
#include "caf/message_handler.hpp"
#include "caf/message_id.hpp"
#include "caf/message_priority.hpp"
#include "caf/response_promise.hpp"
#include "caf/response_type.hpp"
#include "caf/resumable.hpp"
#include "caf/spawn_options.hpp"
#include "caf/telemetry/histogram.hpp"
#include "caf/timespan.hpp"
#include "caf/typed_actor.hpp"
#include "caf/typed_response_promise.hpp"

#include <atomic>
#include <cstdint>
#include <exception>
#include <functional>
#include <type_traits>
#include <utility>

namespace caf {

/// Base class for actors running on this node, either
/// living in an own thread or cooperatively scheduled.
class CAF_CORE_EXPORT local_actor : public abstract_actor {
public:
  // -- member types -----------------------------------------------------------

  /// Defines a monotonic clock suitable for measuring intervals.
  using clock_type = std::chrono::steady_clock;

  /// Optional metrics collected by individual actors when configured to do so.
  struct metrics_t {
    /// Samples how long the actor needs to process messages.
    telemetry::dbl_histogram* processing_time = nullptr;

    /// Samples how long messages wait in the mailbox before being processed.
    telemetry::dbl_histogram* mailbox_time = nullptr;

    /// Counts how many messages are currently waiting in the mailbox.
    telemetry::int_gauge* mailbox_size = nullptr;
  };

  /// Optional metrics for inbound stream traffic collected by individual actors
  /// when configured to do so.
  struct inbound_stream_metrics_t {
    /// Counts the total number of processed stream elements from upstream.
    telemetry::int_counter* processed_elements = nullptr;

    /// Tracks how many stream elements from upstream are currently buffered.
    telemetry::int_gauge* input_buffer_size = nullptr;
  };

  /// Optional metrics for outbound stream traffic collected by individual
  /// actors when configured to do so.
  struct outbound_stream_metrics_t {
    /// Counts the total number of elements that have been pushed downstream.
    telemetry::int_counter* pushed_elements = nullptr;

    /// Tracks how many stream elements are currently waiting in the output
    /// buffer due to insufficient credit.
    telemetry::int_gauge* output_buffer_size = nullptr;
  };

  // -- constructors, destructors, and assignment operators --------------------

  local_actor(actor_config& cfg);

  ~local_actor() override;

  void on_destroy() override;

  // This function performs additional steps to initialize actor-specific
  // metrics. It calls virtual functions and thus cannot run as part of the
  // constructor.
  void setup_metrics();

  // -- pure virtual modifiers -------------------------------------------------

  virtual void launch(execution_unit* eu, bool lazy, bool hide) = 0;

  // -- time -------------------------------------------------------------------

  /// Returns the current time.
  clock_type::time_point now() const noexcept;

  // -- timeout management -----------------------------------------------------

  /// Requests a new timeout for `mid`.
  /// @pre `mid.is_request()`
  disposable request_response_timeout(timespan d, message_id mid);

  // -- spawn functions --------------------------------------------------------

  template <class T, spawn_options Os = no_spawn_options, class... Ts>
  infer_handle_from_class_t<T> spawn(Ts&&... xs) {
    actor_config cfg{context(), this};
    return eval_opts(Os, system().spawn_class<T, make_unbound(Os)>(
                           cfg, std::forward<Ts>(xs)...));
  }

  template <spawn_options Os = no_spawn_options, class F, class... Ts>
  infer_handle_from_fun_t<F> spawn(F fun, Ts&&... xs) {
    using impl = infer_impl_from_fun_t<F>;
    static constexpr bool spawnable = detail::spawnable<F, impl, Ts...>();
    static_assert(spawnable,
                  "cannot spawn function-based actor with given arguments");
    actor_config cfg{context(), this};
    static constexpr spawn_options unbound = make_unbound(Os);
    std::bool_constant<spawnable> enabled;
    return eval_opts(Os, system().spawn_functor<unbound>(
                           enabled, cfg, fun, std::forward<Ts>(xs)...));
  }

  // -- sending asynchronous messages ------------------------------------------

  /// Sends an exit message to `receiver`.
  void send_exit(const actor_addr& receiver, error reason);

  /// Sends an exit message to `receiver`.
  void send_exit(const strong_actor_ptr& receiver, error reason);

  /// Sends an exit message to `receiver`.
  template <class Handle>
  void send_exit(const Handle& receiver, error reason) {
    if (receiver)
      receiver->enqueue(make_mailbox_element(ctrl(), make_message_id(),
                                             exit_msg{address(),
                                                      std::move(reason)}),
                        context());
  }

  template <message_priority Priority = message_priority::normal, class Handle,
            class T, class... Ts>
  void anon_send(const Handle& receiver, T&& arg, Ts&&... args) {
    detail::send_type_check<none_t, Handle, T, Ts...>();
    do_anon_send(actor_cast<abstract_actor*>(receiver), Priority,
                 make_message(std::forward<T>(arg), std::forward<Ts>(args)...));
  }

  template <message_priority Priority = message_priority::normal, class Handle,
            class T, class... Ts>
  disposable scheduled_anon_send(const Handle& receiver,
                                 actor_clock::time_point timeout, T&& arg,
                                 Ts&&... args) {
    detail::send_type_check<none_t, Handle, T, Ts...>();
    return do_scheduled_anon_send(
      actor_cast<strong_actor_ptr>(receiver), Priority, timeout,
      make_message(std::forward<T>(arg), std::forward<Ts>(args)...));
  }

  template <message_priority Priority = message_priority::normal, class Handle,
            class T, class... Ts>
  disposable delayed_anon_send(const Handle& receiver,
                               actor_clock::duration_type timeout, T&& arg,
                               Ts&&... args) {
    return scheduled_anon_send(receiver, clock().now() + timeout,
                               std::forward<T>(arg), std::forward<Ts>(args)...);
  }

  // -- miscellaneous actor operations -----------------------------------------

  /// Returns the execution unit currently used by this actor.
  execution_unit* context() const noexcept {
    return context_;
  }

  /// Sets the execution unit for this actor.
  void context(execution_unit* x) noexcept {
    context_ = x;
  }

  /// Returns the hosting actor system.
  actor_system& system() const noexcept {
    return home_system();
  }

  /// Returns the config of the hosting actor system.
  const actor_system_config& config() const noexcept {
    return system().config();
  }

  /// Returns the clock of the actor system.
  actor_clock& clock() const noexcept {
    return home_system().clock();
  }

  /// @cond PRIVATE

  void monitor(abstract_actor* ptr, message_priority prio);

  /// @endcond

  /// Returns a pointer to the sender of the current message.
  /// @pre `current_mailbox_element() != nullptr`
  strong_actor_ptr& current_sender() noexcept {
    CAF_ASSERT(current_element_);
    return current_element_->sender;
  }

  /// Returns the ID of the current message.
  message_id current_message_id() noexcept {
    CAF_ASSERT(current_element_);
    return current_element_->mid;
  }

  /// Returns the ID of the current message and marks the ID stored in the
  /// current mailbox element as answered.
  message_id take_current_message_id() noexcept {
    CAF_ASSERT(current_element_);
    auto result = current_element_->mid;
    current_element_->mid.mark_as_answered();
    return result;
  }

  /// Marks the current message ID as answered.
  void drop_current_message_id() noexcept {
    CAF_ASSERT(current_element_);
    current_element_->mid.mark_as_answered();
  }

  /// Returns a pointer to the currently processed mailbox element.
  mailbox_element* current_mailbox_element() noexcept {
    return current_element_;
  }

  /// Returns a pointer to the currently processed mailbox element.
  /// @private
  void current_mailbox_element(mailbox_element* ptr) noexcept {
    current_element_ = ptr;
  }

  /// Adds a unidirectional `monitor` to `node`.
  /// @note Each call to `monitor` creates a new, independent monitor.
  void monitor(const node_id& node);

  /// Adds a unidirectional `monitor` to `whom`.
  /// @note Each call to `monitor` creates a new, independent monitor.
  template <message_priority P = message_priority::normal, class Handle>
  void monitor(const Handle& whom) {
    monitor(actor_cast<abstract_actor*>(whom), P);
  }

  /// Removes a monitor from `whom`.
  void demonitor(const actor_addr& whom);

  /// Removes a monitor from `whom`.
  void demonitor(const strong_actor_ptr& whom);

  /// Removes a monitor from `node`.
  void demonitor(const node_id& node);

  /// Removes a monitor from `whom`.
  template <class Handle>
  void demonitor(const Handle& whom) {
    demonitor(whom.address());
  }

  /// Can be overridden to perform cleanup code after an actor
  /// finished execution.
  virtual void on_exit();

  /// Creates a `typed_response_promise` to respond to a request later on.
  /// `make_response_promise<typed_response_promise<int, int>>()`
  /// is equivalent to `make_response_promise<int, int>()`.
  template <class... Ts>
  detail::response_promise_t<Ts...> make_response_promise() {
    using result_t = detail::make_response_promise_helper_t<Ts...>;
    if (current_element_ != nullptr && !current_element_->mid.is_answered()) {
      auto result = result_t{this, *current_element_};
      current_element_->mid.mark_as_answered();
      return result;
    } else {
      return {};
    }
  }

  /// Creates a `response_promise` to respond to a request later on.
  response_promise make_response_promise() {
    return make_response_promise<response_promise>();
  }

  const char* name() const override;

  /// Serializes the state of this actor to `sink`. This function is
  /// only called if this actor has set the `is_serializable` flag.
  /// The default implementation throws a `std::logic_error`.
  virtual error save_state(serializer& sink, unsigned int version);

  /// Deserializes the state of this actor from `source`. This function is
  /// only called if this actor has set the `is_serializable` flag.
  /// The default implementation throws a `std::logic_error`.
  virtual error load_state(deserializer& source, unsigned int version);

  /// Returns the currently defined fail state. If this reason is not
  /// `none` then the actor will terminate with this error after executing
  /// the current message handler.
  const error& fail_state() const noexcept {
    return fail_state_;
  }

  // -- here be dragons: end of public interface -------------------------------

  /// @cond PRIVATE

  auto& builtin_metrics() noexcept {
    return metrics_;
  }

  bool has_metrics_enabled() const noexcept {
    // Either all fields are null or none is.
    return metrics_.processing_time != nullptr;
  }

  template <class ActorHandle>
  ActorHandle eval_opts(spawn_options opts, ActorHandle res) {
    if (has_monitor_flag(opts))
      monitor(res->address());
    if (has_link_flag(opts))
      link_to(res->address());
    return res;
  }

  // returns 0 if last_dequeued() is an asynchronous or sync request message,
  // a response id generated from the request id otherwise
  message_id get_response_id() const {
    auto mid = current_element_->mid;
    return (mid.is_request()) ? mid.response_id() : message_id();
  }

  template <message_priority P = message_priority::normal, class Handle = actor,
            class... Ts>
  typename response_type<
    typename Handle::signatures,
    detail::implicit_conversions_t<std::decay_t<Ts>>...>::delegated_type
  delegate(const Handle& dest, Ts&&... xs) {
    auto rp = make_response_promise();
    return rp.template delegate<P>(dest, std::forward<Ts>(xs)...);
  }

  virtual void initialize();

  bool cleanup(error&& fail_state, execution_unit* host) override;

  message_id new_request_id(message_priority mp) noexcept;

  /// Returns a 64-bit ID that is unique on this actor.
  uint64_t new_u64_id() noexcept;

  template <class T>
  void respond(T& x) {
    response_promise::respond_to(this, current_mailbox_element(), x);
  }

  /// @endcond

protected:
  // -- send functions ---------------------------------------------------------

  /// Sends `msg` as an asynchronous message to `receiver`.
  /// @param receiver The receiver for the message.
  /// @param priority The priority for sending the message.
  /// @param msg The message to send.
  void do_send(abstract_actor* receiver, message_priority priority,
               message&& msg);

  /// Sends `msg` as an asynchronous message to `receiver` after the timeout.
  /// @param receiver The receiver for the message.
  /// @param priority The priority for sending the message.
  /// @param msg The message to send.
  disposable do_scheduled_send(strong_actor_ptr receiver,
                               message_priority priority,
                               actor_clock::time_point timeout, message&& msg);

  /// Sends `msg` as an asynchronous message to `receiver` without sender
  /// information.
  /// @param receiver The receiver for the message.
  /// @param priority The priority for sending the message.
  /// @param msg The message to send.
  void do_anon_send(abstract_actor* receiver, message_priority priority,
                    message&& msg);

  /// Sends `msg` as an asynchronous message to `receiver` after the timeout
  /// without sender information.
  /// @param receiver The receiver for the message.
  /// @param priority The priority for sending the message.
  /// @param msg The message to send.
  disposable
  do_scheduled_anon_send(strong_actor_ptr receiver, message_priority priority,
                         actor_clock::time_point timeout, message&& msg);

  // -- member variables -------------------------------------------------------

  // identifies the execution unit this actor is currently executed by
  execution_unit* context_;

  // pointer to the sender of the currently processed message
  mailbox_element* current_element_;

  // last used request ID
  message_id last_request_id_;

  /// Factory function for returning initial behavior in function-based actors.
  detail::unique_function<behavior(local_actor*)> initial_behavior_fac_;

  metrics_t metrics_;
};

} // namespace caf
