/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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

#ifndef CAF_LOCAL_ACTOR_HPP
#define CAF_LOCAL_ACTOR_HPP

#include <atomic>
#include <cstdint>
#include <exception>
#include <functional>
#include <forward_list>

#include "caf/fwd.hpp"

#include "caf/actor.hpp"
#include "caf/extend.hpp"
#include "caf/message.hpp"
#include "caf/channel.hpp"
#include "caf/duration.hpp"
#include "caf/behavior.hpp"
#include "caf/spawn_fwd.hpp"
#include "caf/resumable.hpp"
#include "caf/actor_cast.hpp"
#include "caf/message_id.hpp"
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
#include "caf/invoke_message_result.hpp"

#include "caf/detail/logging.hpp"
#include "caf/detail/disposer.hpp"
#include "caf/detail/behavior_stack.hpp"
#include "caf/detail/typed_actor_util.hpp"
#include "caf/detail/single_reader_queue.hpp"
#include "caf/detail/memory_cache_flag_type.hpp"

namespace caf {

/// Base class for actors running on this node, either
/// living in an own thread or cooperatively scheduled.
class local_actor : public abstract_actor, public resumable {
public:
  using mailbox_type = detail::single_reader_queue<mailbox_element,
                                                   detail::disposer>;

  static constexpr auto memory_cache_flag = detail::needs_embedding;

  ~local_actor();

  /****************************************************************************
   *                           spawn untyped actors                           *
   ****************************************************************************/

  template <class T, spawn_options Os = no_spawn_options, class... Ts>
  actor spawn(Ts&&... xs) {
    constexpr auto os = make_unbound(Os);
    auto res = spawn_class<T, os>(host(), empty_before_launch_callback{},
                                  std::forward<Ts>(xs)...);
    return eval_opts(Os, std::move(res));
  }

  template <spawn_options Os = no_spawn_options, class... Ts>
  actor spawn(Ts&&... xs) {
    constexpr auto os = make_unbound(Os);
    auto res = spawn_functor<os>(host(), empty_before_launch_callback{},
                                 std::forward<Ts>(xs)...);
    return eval_opts(Os, std::move(res));
  }

  template <class T, spawn_options Os = no_spawn_options, class Groups,
            class... Ts>
  actor spawn_in_groups(const Groups& grps, Ts&&... xs) {
    constexpr auto os = make_unbound(Os);
    auto res = spawn_class<T, os>(host(),
                                  groups_subscriber<
                                    decltype(std::begin(grps))
                                  >{std::begin(grps), std::end(grps)},
                                  std::forward<Ts>(xs)...);
    return eval_opts(Os, std::move(res));
  }

  template <class T, spawn_options Os = no_spawn_options, class... Ts>
  actor spawn_in_groups(std::initializer_list<group> grps, Ts&&... xs) {
    return spawn_in_groups<
      T, Os, std::initializer_list<group>
    >(grps, std::forward<Ts>(xs)...);
  }

  template <class T, spawn_options Os = no_spawn_options, class... Ts>
  actor spawn_in_group(const group& grp, Ts&&... xs) {
    return spawn_in_groups<T, Os>({grp}, std::forward<Ts>(xs)...);
  }

  template <spawn_options Os = no_spawn_options, class Groups, class... Ts>
  actor spawn_in_groups(const Groups& grps, Ts&&... xs) {
    constexpr auto os = make_unbound(Os);
    auto res = spawn_functor<os>(host(),
                                 groups_subscriber<
                                   decltype(std::begin(grps))
                                 >{std::begin(grps), std::end(grps)},
                                 std::forward<Ts>(xs)...);
    return eval_opts(Os, std::move(res));
  }

  template <spawn_options Os = no_spawn_options, class... Ts>
  actor spawn_in_groups(std::initializer_list<group> grps, Ts&&... xs) {
    return spawn_in_groups<
      Os, std::initializer_list<group>
    >(grps, std::forward<Ts>(xs)...);
  }

  template <spawn_options Os = no_spawn_options, class... Ts>
  actor spawn_in_group(const group& grp, Ts&&... xs) {
    return spawn_in_groups<Os>({grp}, std::forward<Ts>(xs)...);
  }

  /****************************************************************************
   *                            spawn typed actors                            *
   ****************************************************************************/

  template <class T, spawn_options Os = no_spawn_options, class... Ts>
  typename actor_handle_from_signature_list<
    typename T::signatures
  >::type
  spawn_typed(Ts&&... xs) {
    constexpr auto os = make_unbound(Os);
    auto res = spawn_class<T, os>(host(), empty_before_launch_callback{},
                    std::forward<Ts>(xs)...);
    return eval_opts(Os, std::move(res));
  }

  template <spawn_options Os = no_spawn_options, typename F, class... Ts>
  typename infer_typed_actor_handle<
    typename detail::get_callable_trait<F>::result_type,
    typename detail::tl_head<
      typename detail::get_callable_trait<F>::arg_types
    >::type
  >::type
  spawn_typed(F fun, Ts&&... xs) {
    constexpr auto os = make_unbound(Os);
    auto res = caf::spawn_typed_functor<os>(host(),
                                            empty_before_launch_callback{},
                                            std::move(fun),
                                            std::forward<Ts>(xs)...);
    return eval_opts(Os, std::move(res));
  }

  /****************************************************************************
   *                        send asynchronous messages                        *
   ****************************************************************************/

  /// Sends `{xs...}` to `dest` using the priority `mp`.
  template <class... Ts>
  void send(message_priority mp, const channel& dest, Ts&&... xs) {
    static_assert(sizeof...(Ts) > 0, "sizeof...(Ts) == 0");
    send_impl(message_id::make(mp), actor_cast<abstract_channel*>(dest),
              std::forward<Ts>(xs)...);
  }

  /// Sends `{xs...}` to `dest` using normal priority.
  template <class... Ts>
  void send(const channel& dest, Ts&&... xs) {
    send_impl(message_id::make(), actor_cast<abstract_channel*>(dest),
              std::forward<Ts>(xs)...);
  }

  /// Sends `{xs...}` to `dest` using the priority `mp`.
  template <class... Sigs, class... Ts>
  void send(message_priority mp, const typed_actor<Sigs...>& dest, Ts&&... xs) {
    using token =
      detail::type_list<
        typename detail::implicit_conversions<
          typename std::decay<Ts>::type
        >::type...>;
    token tk;
    check_typed_input(dest, tk);
    send_impl(message_id::make(mp), actor_cast<abstract_channel*>(dest),
              std::forward<Ts>(xs)...);
  }

  /// Sends `{xs...}` to `dest` using normal priority.
  template <class... Sigs, class... Ts>
  void send(const typed_actor<Sigs...>& dest, Ts&&... xs) {
    using token =
      detail::type_list<
        typename detail::implicit_conversions<
          typename std::decay<Ts>::type
        >::type...>;
    token tk;
    check_typed_input(dest, tk);
    send_impl(message_id::make(), actor_cast<abstract_channel*>(dest),
              std::forward<Ts>(xs)...);
  }

  /// Sends an exit message to `dest`.
  void send_exit(const actor_addr& dest, uint32_t reason);

  /// Sends an exit message to `dest`.
  template <class ActorHandle>
  void send_exit(const ActorHandle& dest, uint32_t reason) {
    send_exit(dest.address(), reason);
  }

  /// Sends a message to `dest` that is delayed by `rel_time`
  /// using the priority `mp`.
  template <class... Ts>
  void delayed_send(message_priority mp, const channel& dest,
                    const duration& rtime, Ts&&... xs) {
    delayed_send_impl(message_id::make(mp), dest, rtime,
                      make_message(std::forward<Ts>(xs)...));
  }

  /// Sends a message to `dest` that is delayed by `rel_time`.
  template <class... Ts>
  void delayed_send(const channel& dest, const duration& rtime, Ts&&... xs) {
    delayed_send_impl(message_id::make(), dest, rtime,
                      make_message(std::forward<Ts>(xs)...));
  }

  /// Sends `{xs...}` delayed by `rtime` to `dest` using the priority `mp`.
  template <class... Sigs, class... Ts>
  void delayed_send(message_priority mp, const typed_actor<Sigs...>& dest,
                    const duration& rtime, Ts&&... xs) {
    using token =
      detail::type_list<
        typename detail::implicit_conversions<
          typename std::decay<Ts>::type
        >::type...>;
    token tk;
    check_typed_input(dest, tk);
    delayed_send_impl(message_id::make(mp), actor_cast<abstract_channel*>(dest),
                      rtime, make_message(std::forward<Ts>(xs)...));
  }


  /// Sends `{xs...}` delayed by `rtime` to `dest` using the priority `mp`.
  template <class... Sigs, class... Ts>
  void delayed_send(const typed_actor<Sigs...>& dest,
                    const duration& rtime, Ts&&... xs) {
    using token =
      detail::type_list<
        typename detail::implicit_conversions<
          typename std::decay<Ts>::type
        >::type...>;
    token tk;
    check_typed_input(dest, tk);
    delayed_send_impl(message_id::make(), actor_cast<abstract_channel*>(dest),
                      rtime, make_message(std::forward<Ts>(xs)...));
  }

  /****************************************************************************
   *                      miscellaneous actor operations                      *
   ****************************************************************************/

  /// Causes this actor to subscribe to the group `what`.
  /// The group will be unsubscribed if the actor finishes execution.
  void join(const group& what);

  /// Causes this actor to leave the group `what`.
  void leave(const group& what);

  /// Finishes execution of this actor after any currently running
  /// message handler is done.
  /// This member function clears the behavior stack of the running actor
  /// and invokes `on_exit()`. The actors does not finish execution
  /// if the implementation of `on_exit()` sets a new behavior.
  /// When setting a new behavior in `on_exit()`, one has to make sure
  /// to not produce an infinite recursion.
  ///
  /// If `on_exit()` did not set a new behavior, the actor sends an
  /// exit message to all of its linked actors, sets its state to exited
  /// and finishes execution.
  ///
  /// In case this actor uses the blocking API, this member function unwinds
  /// the stack by throwing an `actor_exited` exception.
  /// @warning This member function throws immediately in thread-based actors
  ///          that do not use the behavior stack, i.e., actors that use
  ///          blocking API calls such as {@link receive()}.
  void quit(uint32_t reason = exit_reason::normal);

  /// Checks whether this actor traps exit messages.
  inline bool trap_exit() const {
    return get_flag(trap_exit_flag);
  }

  /// Enables or disables trapping of exit messages.
  inline void trap_exit(bool value) {
    set_flag(value, trap_exit_flag);
  }

  /// Returns the currently processed message.
  /// @warning Only set during callback invocation. Calling this member function
  ///          is undefined behavior (dereferencing a `nullptr`) when not in a
  ///          callback or `forward_to` has been called previously.
  inline message& current_message() {
    return current_element_->msg;
  }

  /// Returns the address of the sender of the current message.
  /// @warning Only set during callback invocation. Calling this member function
  ///          is undefined behavior (dereferencing a `nullptr`) when not in a
  ///          callback or `forward_to` has been called previously.
  inline actor_addr& current_sender() {
    return current_element_->sender;
  }

  /// Adds a unidirectional `monitor` to `whom`.
  /// @note Each call to `monitor` creates a new, independent monitor.
  void monitor(const actor_addr& whom);

  /// @copydoc monitor(const actor_addr&)
  inline void monitor(const actor& whom) {
    monitor(whom.address());
  }

  /// @copydoc monitor(const actor_addr&)
  template <class... Ts>
  inline void monitor(const typed_actor<Ts...>& whom) {
    monitor(whom.address());
  }

  /// Removes a monitor from `whom`.
  void demonitor(const actor_addr& whom);

  /// Removes a monitor from `whom`.
  inline void demonitor(const actor& whom) {
    demonitor(whom.address());
  }

  /// Can be overridden to perform cleanup code after an actor
  /// finished execution.
  virtual void on_exit();

  /// Returns all joined groups.
  std::vector<group> joined_groups() const;

  /// Creates a `response_promise` to response to a request later on.
  response_promise make_response_promise();

  /// Sets the handler for unexpected synchronous response messages.
  inline void on_sync_failure(std::function<void()> fun) {
    sync_failure_handler_ = std::move(fun);
  }

  /// Checks wheter this actor has a user-defined sync failure handler.
  inline bool has_sync_failure_handler() const {
    return static_cast<bool>(sync_failure_handler_);
  }

  /// Sets a custom exception handler for this actor. If multiple handlers are
  /// defined, only the functor that was added *last* is being executed.
  template <class F>
  void set_exception_handler(F f) {
    struct functor_attachable : attachable {
      F functor_;
      functor_attachable(F arg) : functor_(std::move(arg)) {
        // nop
      }
      optional<uint32_t> handle_exception(const std::exception_ptr& eptr) {
        return functor_(eptr);
      }
    };
    attach(attachable_ptr{new functor_attachable(std::move(f))});
  }

  /****************************************************************************
   *                       deprecated member functions                        *
   ****************************************************************************/

  // <backward_compatibility version="0.12">
  message& last_dequeued() CAF_DEPRECATED;

  actor_addr& last_sender() CAF_DEPRECATED;
  // </backward_compatibility>

  /****************************************************************************
   *           override pure virtual member functions of resumable            *
   ****************************************************************************/

  void attach_to_scheduler() override;

  void detach_from_scheduler() override;

  resumable::resume_result resume(execution_unit*, size_t) override;

  /****************************************************************************
   *                 here be dragons: end of public interface                 *
   ****************************************************************************/

  /// @cond PRIVATE

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

  inline mailbox_element_ptr& current_mailbox_element() {
    return current_element_;
  }

  inline void handle_sync_failure() {
    if (sync_failure_handler_) {
      sync_failure_handler_();
    } else {
      quit(exit_reason::unhandled_sync_failure);
    }
  }

  template <class Handle, class... Ts>
  message_id sync_send_impl(message_priority mp, const Handle& dh, Ts&&... xs) {
    if (! dh) {
      throw std::invalid_argument("cannot sync_send to invalid_actor");
    }
    auto req_id = new_request_id(mp);
    send_impl(req_id, actor_cast<abstract_channel*>(dh),
              std::forward<Ts>(xs)...);
    return req_id.response_id();
  }

  void request_sync_timeout_msg(const duration& dr, message_id mid);

  // returns 0 if last_dequeued() is an asynchronous or sync request message,
  // a response id generated from the request id otherwise
  inline message_id get_response_id() const {
    auto mid = current_element_->mid;
    return (mid.is_request()) ? mid.response_id() : message_id();
  }

  void forward_message(const actor& dest, message_priority mp);

  inline uint32_t planned_exit_reason() const {
    return planned_exit_reason_;
  }

  inline void planned_exit_reason(uint32_t value) {
    planned_exit_reason_ = value;
  }

  inline detail::behavior_stack& bhvr_stack() {
    return bhvr_stack_;
  }

  inline mailbox_type& mailbox() {
    return mailbox_;
  }

  inline bool has_behavior() const {
    return ! bhvr_stack_.empty() || ! pending_responses_.empty();
  }

  behavior& get_behavior();

  virtual void initialize() = 0;

  void cleanup(uint32_t reason);

  // an actor can have multiple pending timeouts, but only
  // the latest one is active (i.e. the pending_timeouts_.back())

  uint32_t request_timeout(const duration& d);

  void handle_timeout(behavior& bhvr, uint32_t timeout_id);

  void reset_timeout(uint32_t timeout_id);

  // @pre has_timeout()
  bool is_active_timeout(uint32_t tid) const;

  // @pre has_timeout()
  uint32_t active_timeout_id() const;

  invoke_message_result invoke_message(mailbox_element_ptr& node,
                                       behavior& fun,
                                       message_id awaited_response);

  using pending_response = std::pair<message_id, behavior>;

  message_id new_request_id(message_priority mp);

  void mark_arrived(message_id response_id);

  bool awaits_response() const;

  bool awaits(message_id response_id) const;

  optional<pending_response&> find_pending_response(message_id mid);

  void set_response_handler(message_id response_id, behavior bhvr);

  behavior& awaited_response_handler();

  message_id awaited_response_id();

  // these functions are dispatched via the actor policies table

  void launch(execution_unit* eu, bool lazy, bool hide);

  void enqueue(const actor_addr&, message_id,
               message, execution_unit*) override;

  void enqueue(mailbox_element_ptr, execution_unit*) override;

  mailbox_element_ptr next_message();

  bool has_next_message();

  void push_to_cache(mailbox_element_ptr);

  bool invoke_from_cache();

  bool invoke_from_cache(behavior&, message_id);

  inline void initial_behavior_fac(std::function<behavior (local_actor*)> fun) {
    initial_behavior_fac_ = std::move(fun);
  }

protected:
  void do_become(behavior bhvr, bool discard_old);

  // used only in thread-mapped actors
  void await_data();

  // identifies the ID of the last sent synchronous request
  message_id last_request_id_;

  // identifies all IDs of sync messages waiting for a response
  std::forward_list<pending_response> pending_responses_;

  // points to dummy_node_ if no callback is currently invoked,
  // points to the node under processing otherwise
  mailbox_element_ptr current_element_;

  // set by quit
  uint32_t planned_exit_reason_;

  // identifies the timeout messages we are currently waiting for
  uint32_t timeout_id_;

  // used by both event-based and blocking actors
  detail::behavior_stack bhvr_stack_;

  // used by both event-based and blocking actors
  mailbox_type mailbox_;

  // used by functor-based actors to implemented make_behavior() or act()
  std::function<behavior (local_actor*)> initial_behavior_fac_;

  /// @endcond

private:
  template <class T, class... Ts>
  typename std::enable_if<
    ! std::is_same<typename std::decay<T>::type, message>::value
  >::type
  send_impl(message_id mid, abstract_channel* dest, T&& x, Ts&&... xs) const {
    if (! dest) {
      return;
    }
    dest->enqueue(mailbox_element::make_joint(address(),
                                              mid,
                                              std::forward<T>(x),
                                              std::forward<Ts>(xs)...),
                  host());
  }

  void send_impl(message_id mp, abstract_channel* dest, message what) const;

  void delayed_send_impl(message_id mid, const channel& whom,
                         const duration& rtime, message data);

  std::function<void()> sync_failure_handler_;
};

/// A smart pointer to a {@link local_actor} instance.
/// @relates local_actor
using local_actor_ptr = intrusive_ptr<local_actor>;

} // namespace caf

#endif // CAF_LOCAL_ACTOR_HPP
