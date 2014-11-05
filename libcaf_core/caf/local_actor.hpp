/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_CONTEXT_HPP
#define CAF_CONTEXT_HPP

#include <atomic>
#include <cstdint>
#include <exception>
#include <functional>
#include <forward_list>

#include "caf/actor.hpp"
#include "caf/extend.hpp"
#include "caf/message.hpp"
#include "caf/channel.hpp"
#include "caf/duration.hpp"
#include "caf/behavior.hpp"
#include "caf/spawn_fwd.hpp"
#include "caf/message_id.hpp"
#include "caf/match_expr.hpp"
#include "caf/exit_reason.hpp"
#include "caf/typed_actor.hpp"
#include "caf/spawn_options.hpp"
#include "caf/abstract_actor.hpp"
#include "caf/abstract_group.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/message_handler.hpp"
#include "caf/response_promise.hpp"
#include "caf/message_priority.hpp"
#include "caf/check_typed_input.hpp"

#include "caf/mixin/memory_cached.hpp"

#include "caf/detail/logging.hpp"
#include "caf/detail/behavior_stack.hpp"
#include "caf/detail/typed_actor_util.hpp"
#include "caf/detail/single_reader_queue.hpp"

namespace caf {

// forward declarations
class sync_handle_helper;

/**
 * Base class for local running actors.
 * @warning Instances of `local_actor` start with a reference count of 1
 * @extends abstract_actor
 */
class local_actor : public extend<abstract_actor>::with<mixin::memory_cached> {
 public:
  using del = detail::disposer;
  using mailbox_type = detail::single_reader_queue<mailbox_element, del>;

  ~local_actor();

  /**************************************************************************
   *              spawn untyped actors              *
   **************************************************************************/

  template <class C, spawn_options Os = no_spawn_options, class... Ts>
  actor spawn(Ts&&... args) {
    constexpr auto os = make_unbound(Os);
    auto res = spawn_class<C, os>(host(), empty_before_launch_callback{},
                    std::forward<Ts>(args)...);
    return eval_opts(Os, std::move(res));
  }

  template <spawn_options Os = no_spawn_options, class... Ts>
  actor spawn(Ts&&... args) {
    constexpr auto os = make_unbound(Os);
    auto res = spawn_functor<os>(host(), empty_before_launch_callback{},
                   std::forward<Ts>(args)...);
    return eval_opts(Os, std::move(res));
  }

  template <class C, spawn_options Os, class... Ts>
  actor spawn_in_group(const group& grp, Ts&&... args) {
    constexpr auto os = make_unbound(Os);
    auto res = spawn_class<C, os>(host(), group_subscriber{grp},
                    std::forward<Ts>(args)...);
    return eval_opts(Os, std::move(res));
  }

  template <spawn_options Os = no_spawn_options, class... Ts>
  actor spawn_in_group(const group& grp, Ts&&... args) {
    constexpr auto os = make_unbound(Os);
    auto res = spawn_functor<os>(host(), group_subscriber{grp},
                   std::forward<Ts>(args)...);
    return eval_opts(Os, std::move(res));
  }

  /**************************************************************************
   *               spawn typed actors               *
   **************************************************************************/

  template <class C, spawn_options Os = no_spawn_options, class... Ts>
  typename actor_handle_from_signature_list<
    typename C::signatures
  >::type
  spawn_typed(Ts&&... args) {
    constexpr auto os = make_unbound(Os);
    auto res = spawn_class<C, os>(host(), empty_before_launch_callback{},
                    std::forward<Ts>(args)...);
    return eval_opts(Os, std::move(res));
  }

  template <spawn_options Os = no_spawn_options, typename F, class... Ts>
  typename infer_typed_actor_handle<
    typename detail::get_callable_trait<F>::result_type,
    typename detail::tl_head<
      typename detail::get_callable_trait<F>::arg_types
    >::type
  >::type
  spawn_typed(F fun, Ts&&... args) {
    constexpr auto os = make_unbound(Os);
    auto res = caf::spawn_typed_functor<os>(host(),
                                            empty_before_launch_callback{},
                                            std::move(fun),
                                            std::forward<Ts>(args)...);
    return eval_opts(Os, std::move(res));
  }

  /**************************************************************************
   *             send asynchronous messages             *
   **************************************************************************/

  /**
   * Sends `what` to the receiver specified in `dest`.
   */
  void send_tuple(message_priority prio, const channel& whom, message what);

  /**
   * Sends `what` to the receiver specified in `dest`.
   */
  inline void send_tuple(const channel& whom, message what) {
    send_tuple(message_priority::normal, whom, std::move(what));
  }

  /**
   * Sends `{what...} to `whom` using the priority `prio`.
   */
  template <class... Ts>
  inline void send(message_priority prio, const channel& whom, Ts&&... what) {
    static_assert(sizeof...(Ts) > 0, "sizeof...(Ts) == 0");
    send_tuple(prio, whom, make_message(std::forward<Ts>(what)...));
  }

  /**
   * Sends `{what...} to `whom`.
   */
  template <class... Ts>
  inline void send(const channel& whom, Ts&&... what) {
    static_assert(sizeof...(Ts) > 0, "sizeof...(Ts) == 0");
    send_tuple(message_priority::normal, whom,
           make_message(std::forward<Ts>(what)...));
  }

  /**
   * Sends `{what...} to `whom`.
   */
  template <class... Rs, class... Ts>
  void send(const typed_actor<Rs...>& whom, Ts... what) {
    check_typed_input(whom,
                      detail::type_list<typename detail::implicit_conversions<
                        typename std::decay<Ts>::type
                      >::type...>{});
    send_tuple(message_priority::normal, actor{whom.m_ptr.get()},
               make_message(std::forward<Ts>(what)...));
  }

  /**
   * Sends an exit message to `whom`.
   */
  void send_exit(const actor_addr& whom, uint32_t reason);

  /**
   * Sends an exit message to `whom`.
   */
  inline void send_exit(const actor& whom, uint32_t reason) {
    send_exit(whom.address(), reason);
  }

  /**
   * Sends an exit message to `whom`.
   */
  template <class... Rs>
  void send_exit(const typed_actor<Rs...>& whom, uint32_t reason) {
    send_exit(whom.address(), reason);
  }

  /**
   * Sends a message to `whom` with priority `prio`
   * that is delayed by `rel_time`.
   */
  void delayed_send_tuple(message_priority prio, const channel& whom,
                          const duration& rtime, message data);

  /**
   * Sends a message to `whom` that is delayed by `rel_time`.
   */
  inline void delayed_send_tuple(const channel& whom, const duration& rtime,
                                 message data) {
    delayed_send_tuple(message_priority::normal, whom, rtime, std::move(data));
  }

  /**
   * Sends a message to `whom` using priority `prio`
   * that is delayed by `rel_time`.
   */
  template <class... Ts>
  void delayed_send(message_priority prio, const channel& whom,
                    const duration& rtime, Ts&&... args) {
    delayed_send_tuple(prio, whom, rtime,
                       make_message(std::forward<Ts>(args)...));
  }

  /**
   * Sends a message to `whom` that is delayed by `rel_time`.
   */
  template <class... Ts>
  void delayed_send(const channel& whom, const duration& rtime, Ts&&... args) {
    delayed_send_tuple(message_priority::normal, whom, rtime,
                       make_message(std::forward<Ts>(args)...));
  }

  /**************************************************************************
   *                     miscellaneous actor operations                     *
   **************************************************************************/

  /**
   * Causes this actor to subscribe to the group `what`.
   * The group will be unsubscribed if the actor finishes execution.
   */
  void join(const group& what);

  /**
   * Causes this actor to leave the group `what`.
   */
  void leave(const group& what);

  /**
   * Finishes execution of this actor after any currently running
   * message handler is done.
   * This member function clears the behavior stack of the running actor
   * and invokes `on_exit()`. The actors does not finish execution
   * if the implementation of `on_exit()` sets a new behavior.
   * When setting a new behavior in `on_exit()`, one has to make sure
   * to not produce an infinite recursion.
   *
   * If `on_exit()` did not set a new behavior, the actor sends an
   * exit message to all of its linked actors, sets its state to exited
   * and finishes execution.
   *
   * In case this actor uses the blocking API, this member function unwinds
   * the stack by throwing an `actor_exited` exception.
   * @warning This member function throws immediately in thread-based actors
   *          that do not use the behavior stack, i.e., actors that use
   *          blocking API calls such as {@link receive()}.
   */
  void quit(uint32_t reason = exit_reason::normal);

  /**
   * Checks whether this actor traps exit messages.
   */
  inline bool trap_exit() const {
    return get_flag(trap_exit_flag);
  }

  /**
   * Enables or disables trapping of exit messages.
   */
  inline void trap_exit(bool value) {
    set_flag(value, trap_exit_flag);
  }

  inline bool has_timeout() const {
    return get_flag(has_timeout_flag);
  }

  inline void has_timeout(bool value) {
    set_flag(value, has_timeout_flag);
  }

  inline bool is_registered() const {
    return get_flag(is_registered_flag);
  }

  void is_registered(bool value);

  inline bool is_initialized() const {
    return get_flag(is_initialized_flag);
  }

  inline void is_initialized(bool value) {
    set_flag(value, is_initialized_flag);
  }

  inline bool is_blocking() const {
    return get_flag(is_blocking_flag);
  }

  inline void is_blocking(bool value) {
    set_flag(value, is_blocking_flag);
  }

  /**
   * Returns the last message that was dequeued from the actor's mailbox.
   * @warning Only set during callback invocation.
   */
  inline message& last_dequeued() {
    return m_current_node->msg;
  }

  /**
   * Returns the address of the last sender of the last dequeued message.
   */
  inline actor_addr& last_sender() {
    return m_current_node->sender;
  }

  /**
   * Adds a unidirectional `monitor` to `whom`.
   * @note Each call to `monitor` creates a new, independent monitor.
   */
  void monitor(const actor_addr& whom);

  /**
   * @copydoc monitor(const actor_addr&)
   */
  inline void monitor(const actor& whom) {
    monitor(whom.address());
  }

  /**
   * @copydoc monitor(const actor_addr&)
   */
  template <class... Rs>
  inline void monitor(const typed_actor<Rs...>& whom) {
    monitor(whom.address());
  }

  /**
   * Removes a monitor from `whom`.
   */
  void demonitor(const actor_addr& whom);

  /**
   * Removes a monitor from `whom`.
   */
  inline void demonitor(const actor& whom) {
    demonitor(whom.address());
  }

  /**
   * Can be overridden to perform cleanup code after an actor
   * finished execution.
   */
  inline void on_exit() {
    // nop
  }

  /**
   * Returns all joined groups.
   */
  std::vector<group> joined_groups() const;

  /**
   * Creates a `response_promise` to response to a request later on.
   */
  response_promise make_response_promise();

  /**
   * Sets the handler for unexpected synchronous response messages.
   */
  inline void on_sync_timeout(std::function<void()> fun) {
    m_sync_timeout_handler = std::move(fun);
  }

  /**
   * Sets the handler for `timed_sync_send` timeout messages.
   */
  inline void on_sync_failure(std::function<void()> fun) {
    m_sync_failure_handler = std::move(fun);
  }

  /**
   * Checks wheter this actor has a user-defined sync failure handler.
   */
  inline bool has_sync_failure_handler() {
    return static_cast<bool>(m_sync_failure_handler);
  }

  /**
   * Calls `on_sync_timeout(fun); on_sync_failure(fun);.
   */
  inline void on_sync_timeout_or_failure(std::function<void()> fun) {
    on_sync_timeout(fun);
    on_sync_failure(fun);
  }

  /**
   * Sets a custom exception handler for this actor. If multiple handlers are
   * defined, only the functor that was added *last* is being executed.
   */
  template <class F>
  void set_exception_handler(F f) {
    struct functor_attachable : attachable {
      F m_functor;
      functor_attachable(F arg) : m_functor(std::move(arg)) {
        // nop
      }
      optional<uint32_t> handle_exception(const std::exception_ptr& eptr) {
        return m_functor(eptr);
      }
    };
    attach(attachable_ptr{new functor_attachable(std::move(f))});
  }

  /**************************************************************************
   *                here be dragons: end of public interface                *
   **************************************************************************/

  /** @cond PRIVATE */

  local_actor();

  template <class ActorHandle>
  inline ActorHandle eval_opts(spawn_options opts, ActorHandle res) {
    if (has_monitor_flag(opts)) {
      monitor(res->address());
    }
    if (has_link_flag(opts)) {
      link_to(res->address());
    }
    return res;
  }

  inline void current_node(mailbox_element* ptr) {
    this->m_current_node = ptr;
  }

  inline mailbox_element* current_node() {
    return this->m_current_node;
  }

  inline message_id new_request_id() {
    auto result = ++m_last_request_id;
    m_pending_responses.push_front(result.response_id());
    return result;
  }

  inline void handle_sync_timeout() {
    if (m_sync_timeout_handler) {
      m_sync_timeout_handler();
    } else {
      quit(exit_reason::unhandled_sync_timeout);
    }
  }

  inline void handle_sync_failure() {
    if (m_sync_failure_handler) {
      m_sync_failure_handler();
    } else {
      quit(exit_reason::unhandled_sync_failure);
    }
  }

  // returns the response ID
  message_id timed_sync_send_tuple_impl(message_priority mp,
                                        const actor& whom,
                                        const duration& rel_time,
                                        message&& what);

  // returns the response ID
  message_id sync_send_tuple_impl(message_priority mp,
                                  const actor& whom,
                                  message&& what);

  // returns the response ID
  template <class... Rs, class... Ts>
  message_id sync_send_tuple_impl(message_priority mp,
                                  const typed_actor<Rs...>& whom,
                                  message&& msg) {
    return sync_send_tuple_impl(mp, actor{whom.m_ptr.get()}, std::move(msg));
  }

  // returns 0 if last_dequeued() is an asynchronous or sync request message,
  // a response id generated from the request id otherwise
  inline message_id get_response_id() {
    auto id = m_current_node->mid;
    return (id.is_request()) ? id.response_id() : message_id();
  }

  void reply_message(message&& what);

  void forward_message(const actor& new_receiver, message_priority prio);

  inline bool awaits(message_id response_id) {
    CAF_REQUIRE(response_id.is_response());
    return std::any_of(m_pending_responses.begin(), m_pending_responses.end(),
                       [=](message_id other) { return response_id == other; });
  }

  inline void mark_arrived(message_id response_id) {
    m_pending_responses.remove(response_id);
  }

  inline uint32_t planned_exit_reason() const {
    return m_planned_exit_reason;
  }

  inline void planned_exit_reason(uint32_t value) {
    m_planned_exit_reason = value;
  }

  void cleanup(uint32_t reason);

  inline mailbox_element* dummy_node() {
    return &m_dummy_node;
  }

  template <class... Ts>
  inline mailbox_element* new_mailbox_element(Ts&&... args) {
    return mailbox_element::create(std::forward<Ts>(args)...);
  }

 protected:
  // identifies the ID of the last sent synchronous request
  message_id m_last_request_id;

  // identifies all IDs of sync messages waiting for a response
  std::forward_list<message_id> m_pending_responses;

  // "default value" for m_current_node
  mailbox_element m_dummy_node;

  // points to m_dummy_node if no callback is currently invoked,
  // points to the node under processing otherwise
  mailbox_element* m_current_node;

  // set by quit
  uint32_t m_planned_exit_reason;

  /** @endcond */

 private:
  using super = combined_type;
  std::function<void()> m_sync_failure_handler;
  std::function<void()> m_sync_timeout_handler;
};

/**
 * A smart pointer to a {@link local_actor} instance.
 * @relates local_actor
 */
using local_actor_ptr = intrusive_ptr<local_actor>;

} // namespace caf

#endif // CAF_CONTEXT_HPP
