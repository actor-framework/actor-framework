/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include "caf/actor_traits.hpp"
#include "caf/config.hpp"
#include "caf/mixin/requester.hpp"
#include "caf/mixin/sender.hpp"
#include "caf/scheduled_actor.hpp"
#include "caf/timespan.hpp"
#include "caf/typed_actor_view_base.hpp"

namespace caf {

/// Decorates a pointer to a @ref scheduled_actor with a statically typed actor
/// interface.
template <class... Sigs>
class typed_actor_view
  : public extend<typed_actor_view_base, typed_actor_view<Sigs...>>::
      template with<mixin::sender, mixin::requester> {
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
  typename infer_handle_from_class<T>::type spawn(Ts&&... xs) {
    return self_->spawn<T, Os>(std::forward<Ts>(xs)...);
  }

  /// @copydoc local_actor::spawn
  template <spawn_options Os = no_spawn_options, class F, class... Ts>
  typename infer_handle_from_fun<F>::type spawn(F fun, Ts&&... xs) {
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

  /// @copydoc scheduled_actor::stream_managers
  auto& stream_managers() noexcept {
    return self_->stream_managers();
  }

  /// @copydoc scheduled_actor::pending_stream_managers
  auto& pending_stream_managers() noexcept {
    return self_->pending_stream_managers();
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

  /// @copydoc monitorable_actor::link_to
  template <class ActorHandle>
  void link_to(const ActorHandle& x) {
    self_->link_to(x);
  }

  /// @copydoc monitorable_actor::unlink_from
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

  // -- miscellaneous actor operations -----------------------------------------

  void quit(exit_reason reason = exit_reason::normal) {
    self_->quit(reason);
  }

  template <class... Ts>
  typename detail::make_response_promise_helper<Ts...>::type
  make_response_promise() {
    return self_->make_response_promise<Ts...>();
  }

  message_id new_request_id(message_priority mp) {
    return self_->new_request_id(mp);
  }

  void request_response_timeout(timespan d, message_id mid) {
    return self_->request_response_timeout(d, mid);
  }

  response_promise make_response_promise() {
    return self_->make_response_promise();
  }

  template <class... Ts,
            class R = typename detail::make_response_promise_helper<
              typename std::decay<Ts>::type...>::type>
  R response(Ts&&... xs) {
    return self_->response(std::forward<Ts>(xs)...);
  }

  template <class... Ts>
  void eq_impl(Ts&&... xs) {
    self_->eq_impl(std::forward<Ts>(xs)...);
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
    ;
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
