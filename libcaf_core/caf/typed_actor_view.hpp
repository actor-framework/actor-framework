// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/actor_traits.hpp"
#include "caf/config.hpp"
#include "caf/extend.hpp"
#include "caf/mixin/requester.hpp"
#include "caf/none.hpp"
#include "caf/scheduled_actor.hpp"
#include "caf/stream.hpp"
#include "caf/timespan.hpp"
#include "caf/typed_actor_view_base.hpp"
#include "caf/typed_stream.hpp"

namespace caf {

/// Utility function to force the type of `self` to depend on `T` and to raise a
/// compiler error if the user did not include 'caf/scheduled_actor/flow.hpp'.
/// The function itself does nothing and simply returns `self`.
template <class T>
auto typed_actor_view_flow_access(caf::scheduled_actor* self) {
  using Self = flow::assert_scheduled_actor_hdr_t<T, caf::scheduled_actor*>;
  return static_cast<Self>(self);
}

/// Decorates a pointer to a @ref scheduled_actor with a statically typed actor
/// interface.
template <class... Sigs>
class typed_actor_view
  : public extend<typed_actor_view_base,
                  typed_actor_view<Sigs...>>::template with<mixin::requester> {
public:
  /// Stores the template parameter pack.
  using signatures = detail::type_list<Sigs...>;

  using pointer = scheduled_actor*;

  explicit typed_actor_view(scheduled_actor* ptr) : self_(ptr) {
    // nop
  }

  // -- spawn functions --------------------------------------------------------

  /// @copydoc local_actor::spawn
  template <class T, spawn_options Os = no_spawn_options, class... Ts>
  infer_handle_from_class_t<T> spawn(Ts&&... xs) {
    return self_->spawn<T, Os>(std::forward<Ts>(xs)...);
  }

  /// @copydoc local_actor::spawn
  template <spawn_options Os = no_spawn_options, class F, class... Ts>
  infer_handle_from_fun_t<F> spawn(F fun, Ts&&... xs) {
    return self_->spawn<Os>(std::move(fun), std::forward<Ts>(xs)...);
  }

  // -- state modifiers --------------------------------------------------------

  /// @copydoc scheduled_actor::quit
  void quit(error x = error{}) {
    self_->quit(std::move(x));
  }

  // -- properties -------------------------------------------------------------

  /// @copydoc abstract_actor::address
  auto address() const noexcept {
    return self_->address();
  }

  /// @copydoc abstract_actor::id
  auto id() const noexcept {
    return self_->id();
  }

  /// @copydoc abstract_actor::node
  auto node() const noexcept {
    return self_->node();
  }

  /// @copydoc abstract_actor::home_system
  auto& home_system() const noexcept {
    return self_->home_system();
  }

  /// @copydoc local_actor::context
  auto context() const noexcept {
    return self_->context();
  }

  /// @copydoc local_actor::system
  auto& system() const noexcept {
    return self_->system();
  }

  /// @copydoc local_actor::config
  const auto& config() const noexcept {
    return self_->config();
  }

  /// @copydoc local_actor::clock
  auto& clock() const noexcept {
    return self_->clock();
  }

  /// @copydoc local_actor::current_sender
  auto& current_sender() noexcept {
    return self_->current_sender();
  }

  /// @copydoc local_actor::current_message_id
  auto current_message_id() noexcept {
    return self_->current_message_id();
  }

  /// @copydoc local_actor::current_mailbox_element
  auto* current_mailbox_element() {
    return self_->current_mailbox_element();
  }

  /// @copydoc local_actor::fail_state
  const auto& fail_state() const {
    return self_->fail_state();
  }

  /// @copydoc scheduled_actor::mailbox
  auto& mailbox() noexcept {
    return self_->mailbox();
  }

  // -- event handlers ---------------------------------------------------------

  /// @copydoc scheduled_actor::set_default_handler
  template <class Fun>
  void set_default_handler(Fun&& fun) {
    self_->set_default_handler(std::forward<Fun>(fun));
  }

  /// @copydoc scheduled_actor::set_error_handler
  template <class Fun>
  void set_error_handler(Fun&& fun) {
    self_->set_error_handler(std::forward<Fun>(fun));
  }

  /// @copydoc scheduled_actor::set_down_handler
  template <class Fun>
  void set_down_handler(Fun&& fun) {
    self_->set_down_handler(std::forward<Fun>(fun));
  }

  /// @copydoc scheduled_actor::set_node_down_handler
  template <class Fun>
  void set_node_down_handler(Fun&& fun) {
    self_->set_node_down_handler(std::forward<Fun>(fun));
  }

  /// @copydoc scheduled_actor::set_exit_handler
  template <class Fun>
  void set_exit_handler(Fun&& fun) {
    self_->set_exit_handler(std::forward<Fun>(fun));
  }

#ifdef CAF_ENABLE_EXCEPTIONS

  /// @copydoc scheduled_actor::set_exception_handler
  template <class Fun>
  void set_exception_handler(Fun&& fun) {
    self_->set_exception_handler(std::forward<Fun>(fun));
  }

#endif // CAF_ENABLE_EXCEPTIONS

  // -- linking and monitoring -------------------------------------------------

  /// @copydoc abstract_actor::link_to
  template <class ActorHandle>
  void link_to(const ActorHandle& x) {
    self_->link_to(x);
  }

  /// @copydoc abstract_actor::unlink_from
  template <class ActorHandle>
  void unlink_from(const ActorHandle& x) {
    self_->unlink_from(x);
  }

  /// @copydoc local_actor::monitor
  void monitor(const node_id& node) {
    self_->monitor(node);
  }

  /// @copydoc local_actor::monitor
  template <message_priority P = message_priority::normal, class Handle>
  void monitor(const Handle& whom) {
    self_->monitor(whom);
  }

  /// @copydoc local_actor::demonitor
  void demonitor(const node_id& node) {
    self_->demonitor(node);
  }

  /// @copydoc local_actor::demonitor
  template <class Handle>
  void demonitor(const Handle& whom) {
    self_->demonitor(whom);
  }

  // -- sending asynchronous messages ------------------------------------------

  /// @copydoc local_actor::send_exit
  template <class ActorHandle>
  void send_exit(const ActorHandle& whom, error reason) {
    self_->send_exit(whom, std::move(reason));
  }

  // -- scheduling actions -----------------------------------------------------

  /// @copydoc scheduled_actor::run_scheduled
  template <class Clock, class Duration, class F>
  disposable
  run_scheduled(std::chrono::time_point<Clock, Duration> when, F what) {
    return self_->run_scheduled(when, std::move(what));
  }

  /// @copydoc scheduled_actor::run_scheduled_weak
  template <class Clock, class Duration, class F>
  disposable
  run_scheduled_weak(std::chrono::time_point<Clock, Duration> when, F what) {
    return self_->run_scheduled_weak(when, std::move(what));
  }

  /// @copydoc scheduled_actor::run_delayed
  template <class Rep, class Period, class F>
  disposable run_delayed(std::chrono::duration<Rep, Period> delay, F what) {
    return self_->run_delayed(delay, std::move(what));
  }

  /// @copydoc scheduled_actor::run_delayed_weak
  template <class Rep, class Period, class F>
  disposable
  run_delayed_weak(std::chrono::duration<Rep, Period> delay, F what) {
    return self_->run_delayed_weak(delay, std::move(what));
  }

  // -- miscellaneous actor operations -----------------------------------------

  void quit(exit_reason reason = exit_reason::normal) {
    self_->quit(reason);
  }

  template <class... Ts>
  detail::make_response_promise_helper_t<Ts...> make_response_promise() {
    return self_->make_response_promise<Ts...>();
  }

  message_id new_request_id(message_priority mp) {
    return self_->new_request_id(mp);
  }

  disposable request_response_timeout(timespan d, message_id mid) {
    return self_->request_response_timeout(d, mid);
  }

  response_promise make_response_promise() {
    return self_->make_response_promise();
  }

  void add_awaited_response_handler(message_id response_id, behavior bhvr) {
    return self_->add_awaited_response_handler(response_id, std::move(bhvr));
  }

  void add_multiplexed_response_handler(message_id response_id, behavior bhvr) {
    return self_->add_multiplexed_response_handler(response_id,
                                                   std::move(bhvr));
  }

  template <class Handle, class... Ts>
  auto delegate(const Handle& dest, Ts&&... xs) {
    return self_->delegate(dest, std::forward<Ts>(xs)...);
  }

  /// @private
  actor_control_block* ctrl() const noexcept {
    CAF_ASSERT(self_ != nullptr);
    return actor_control_block::from(self_);
  }

  /// @private
  scheduled_actor* internal_ptr() const noexcept {
    return self_;
  }

  /// @private
  void reset(scheduled_actor* ptr) {
    self_ = ptr;
  }

  operator scheduled_actor*() const noexcept {
    return self_;
  }

  // -- flow API ---------------------------------------------------------------

  /// @copydoc flow::coordinator::make_observable
  template <class T = none_t>
  auto make_observable() {
    // Note: the template parameter T serves no purpose other than forcing the
    //       compiler to delay evaluation of this function body by having
    //       *something* to pass to `typed_actor_view_flow_access`.
    auto self = typed_actor_view_flow_access<T>(self_);
    return self->make_observable();
  }

  /// @copydoc scheduled_actor::observe
  template <class T>
  auto
  observe(typed_stream<T> what, size_t buf_capacity, size_t demand_threshold) {
    auto self = typed_actor_view_flow_access<T>(self_);
    return self->observe(std::move(what), buf_capacity, demand_threshold);
  }

  /// @copydoc scheduled_actor::observe_as
  template <class T>
  auto observe_as(stream what, size_t buf_capacity, size_t demand_threshold) {
    auto self = typed_actor_view_flow_access<T>(self_);
    return self->template observe_as<T>(std::move(what), buf_capacity,
                                        demand_threshold);
  }

  /// @copydoc scheduled_actor::deregister_stream
  void deregister_stream(uint64_t stream_id) {
    self_->deregister_stream(stream_id);
  }

private:
  scheduled_actor* self_;
};

template <class... Sigs>
struct actor_traits<typed_actor_view<Sigs...>> {
  static constexpr bool is_dynamically_typed = false;

  static constexpr bool is_statically_typed = true;

  static constexpr bool is_blocking = false;

  static constexpr bool is_non_blocking = true;

  static constexpr bool is_incomplete = false;
};

} // namespace caf
