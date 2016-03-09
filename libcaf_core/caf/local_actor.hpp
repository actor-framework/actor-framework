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
#include <utility>
#include <exception>
#include <functional>
#include <type_traits>
#include <forward_list>
#include <unordered_map>

#include "caf/fwd.hpp"

#include "caf/actor.hpp"
#include "caf/extend.hpp"
#include "caf/logger.hpp"
#include "caf/message.hpp"
#include "caf/channel.hpp"
#include "caf/duration.hpp"
#include "caf/behavior.hpp"
#include "caf/delegated.hpp"
#include "caf/resumable.hpp"
#include "caf/actor_cast.hpp"
#include "caf/message_id.hpp"
#include "caf/exit_reason.hpp"
#include "caf/typed_actor.hpp"
#include "caf/actor_config.hpp"
#include "caf/actor_system.hpp"
#include "caf/spawn_options.hpp"
#include "caf/abstract_actor.hpp"
#include "caf/abstract_group.hpp"
#include "caf/execution_unit.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/message_handler.hpp"
#include "caf/response_promise.hpp"
#include "caf/message_priority.hpp"
#include "caf/check_typed_input.hpp"
#include "caf/monitorable_actor.hpp"
#include "caf/invoke_message_result.hpp"
#include "caf/typed_response_promise.hpp"

#include "caf/detail/disposer.hpp"
#include "caf/detail/behavior_stack.hpp"
#include "caf/detail/typed_actor_util.hpp"
#include "caf/detail/single_reader_queue.hpp"
#include "caf/detail/memory_cache_flag_type.hpp"

namespace caf {

namespace detail {

template <class... Ts>
struct make_response_promise_helper {
  using type = typed_response_promise<Ts...>;
};

template <class... Ts>
struct make_response_promise_helper<typed_response_promise<Ts...>>
    : make_response_promise_helper<Ts...> {};

template <>
struct make_response_promise_helper<response_promise> {
  using type = response_promise;
};

} // namespace detail

/// Base class for actors running on this node, either
/// living in an own thread or cooperatively scheduled.
class local_actor : public monitorable_actor, public resumable {
public:
  using mailbox_type = detail::single_reader_queue<mailbox_element,
                                                   detail::disposer>;

  static constexpr auto memory_cache_flag = detail::needs_embedding;

  ~local_actor();

  /****************************************************************************
   *                           spawn actors                                   *
   ****************************************************************************/

  template <class T, spawn_options Os = no_spawn_options, class... Ts>
  typename infer_handle_from_class<T>::type
  spawn(Ts&&... xs) {
    actor_config cfg{context()};
    return eval_opts(Os, system().spawn_class<T, make_unbound(Os)>(cfg, std::forward<Ts>(xs)...));
  }

  template <spawn_options Os = no_spawn_options, class F, class... Ts>
  typename infer_handle_from_fun<F>::type
  spawn(F fun, Ts&&... xs) {
    actor_config cfg{context()};
    return eval_opts(Os, system().spawn_functor<make_unbound(Os)>(cfg, fun, std::forward<Ts>(xs)...));
  }

  template <class T, spawn_options Os = no_spawn_options, class Groups,
            class... Ts>
  actor spawn_in_groups(const Groups& gs, Ts&&... xs) {
    actor_config cfg{context()};
    return eval_opts(Os, system().spawn_in_groups_impl<T, make_unbound(Os)>(cfg, gs.begin(), gs.end(), std::forward<Ts>(xs)...));
  }

  template <class T, spawn_options Os = no_spawn_options, class... Ts>
  actor spawn_in_groups(std::initializer_list<group> gs, Ts&&... xs) {
    actor_config cfg{context()};
    return eval_opts(Os, system().spawn_in_groups_impl<T, make_unbound(Os)>(cfg, gs.begin(), gs.end(), std::forward<Ts>(xs)...));
  }

  template <class T, spawn_options Os = no_spawn_options, class... Ts>
  actor spawn_in_group(const group& grp, Ts&&... xs) {
    actor_config cfg{context()};
    auto first = &grp;
    return eval_opts(Os, system().spawn_in_groups_impl<T, make_unbound(Os)>(cfg, first, first + 1, std::forward<Ts>(xs)...));
  }

  template <spawn_options Os = no_spawn_options, class Groups, class F, class... Ts>
  actor spawn_in_groups(const Groups& gs, F fun, Ts&&... xs) {
    actor_config cfg{context()};
    return eval_opts(Os, system().spawn_in_groups_impl<make_unbound(Os)>(cfg, gs.begin(), gs.end(), fun, std::forward<Ts>(xs)...));
  }

  template <spawn_options Os = no_spawn_options, class F, class... Ts>
  actor spawn_in_groups(std::initializer_list<group> gs, F fun, Ts&&... xs) {
    actor_config cfg{context()};
    return eval_opts(Os, system().spawn_in_groups_impl<make_unbound(Os)>(cfg, gs.begin(), gs.end(), fun, std::forward<Ts>(xs)...));
  }

  template <spawn_options Os = no_spawn_options, class F, class... Ts>
  actor spawn_in_group(const group& grp, F fun, Ts&&... xs) {
    actor_config cfg{context()};
    auto first = &grp;
    return eval_opts(Os, system().spawn_in_groups_impl<make_unbound(Os)>(cfg, first, first + 1, fun, std::forward<Ts>(xs)...));
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
    send_impl(message_id::make(mp), actor_cast<abstract_actor*>(dest),
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
    send_impl(message_id::make(), actor_cast<abstract_actor*>(dest),
              std::forward<Ts>(xs)...);
  }

  /// Sends an exit message to `dest`.
  void send_exit(const actor_addr& dest, exit_reason reason);

  /// Sends an exit message to `dest`.
  template <class ActorHandle>
  void send_exit(const ActorHandle& dest, exit_reason reason) {
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
    delayed_send_impl(message_id::make(), actor_cast<channel>(dest),
                      rtime, make_message(std::forward<Ts>(xs)...));
  }

  /****************************************************************************
   *                      miscellaneous actor operations                      *
   ****************************************************************************/

  /// Returns the execution unit currently used by this actor.
  /// @warning not thread safe
  inline execution_unit* context() const {
    return context_;
  }

  /// Sets the execution unit for this actor.
  inline void context(execution_unit* x) {
    context_ = x;
  }

  /// Returns the hosting actor system.
  inline actor_system& system() const {
    CAF_ASSERT(context_);
    return context_->system();
  }

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
  void quit(exit_reason reason = exit_reason::normal);

  /// Checks whether this actor traps exit messages.
  inline bool trap_exit() const {
    return get_flag(trap_exit_flag);
  }

  /// Enables or disables trapping of exit messages.
  inline void trap_exit(bool value) {
    set_flag(value, trap_exit_flag);
  }

  /// @cond PRIVATE
  /// Returns the currently processed message.
  /// @warning Only set during callback invocation. Calling this member function
  ///          is undefined behavior (dereferencing a `nullptr`) when not in a
  ///          callback or `forward_to` has been called previously.
  inline message& current_message() {
    return current_element_->msg;
  }
  /// @endcond

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

  /// Creates a `typed_response_promise` to respond to a request later on.
  /// `make_response_promise<typed_response_promise<int, int>>()`
  /// is equivalent to `make_response_promise<int, int>()`.
  template <class... Ts>
  typename detail::make_response_promise_helper<Ts...>::type
  make_response_promise() {
    auto& ptr = current_element_;
    if (! ptr)
      return {};
    auto& mid = ptr->mid;
    if (mid.is_answered())
      return {};
    return {this, *ptr};
  }

  /// Creates a `response_promise` to respond to a request later on.
  inline response_promise make_response_promise() {
    return make_response_promise<response_promise>();
  }

  /// Creates a `typed_response_promise` and responds immediately.
  /// Return type is deduced from arguments.
  /// Return value is implicitly convertible to untyped response promise.
  template <class... Ts,
            class R =
              typename detail::make_response_promise_helper<
                typename std::decay<Ts>::type...
              >::type>
  R response(Ts&&... xs) {
    auto promise = make_response_promise<R>();
    promise.deliver(std::forward<Ts>(xs)...);
    return promise;
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
      maybe<exit_reason> handle_exception(const std::exception_ptr& eptr) {
        return functor_(eptr);
      }
    };
    attach(attachable_ptr{new functor_attachable(std::move(f))});
  }

  const char* name() const override;

  /// Serializes the state of this actor to `sink`. This function is
  /// only called if this actor has set the `is_serializable` flag.
  /// The default implementation throws a `std::logic_error`.
  virtual void save_state(serializer& sink, const unsigned int version);

  /// Deserializes the state of this actor from `source`. This function is
  /// only called if this actor has set the `is_serializable` flag.
  /// The default implementation throws a `std::logic_error`.
  virtual void load_state(deserializer& source, const unsigned int version);

  /****************************************************************************
   *           override pure virtual member functions of resumable            *
   ****************************************************************************/

  subtype_t subtype() const override;

  ref_counted* as_ref_counted_ptr() override;

  resume_result resume(execution_unit*, size_t) override;

  /****************************************************************************
   *                 here be dragons: end of public interface                 *
   ****************************************************************************/

  /// @cond PRIVATE

  // handle `ptr` in an event-based actor
  std::pair<resumable::resume_result, invoke_message_result>
  exec_event(mailbox_element_ptr& ptr);

  // handle `ptr` in an event-based actor, not suitable to be called in a loop
  virtual void exec_single_event(execution_unit* ctx, mailbox_element_ptr& ptr);

  local_actor(actor_config& sys);

  local_actor(actor_system* sys, actor_id aid, node_id nid);

  local_actor(actor_system* sys, actor_id aid, node_id nid, int init_flags);

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

  template <class Handle, class... Ts>
  message_id request_impl(message_priority mp, const Handle& dh, Ts&&... xs) {
    if (! dh)
      throw std::invalid_argument("cannot request to invalid_actor");
    auto req_id = new_request_id(mp);
    send_impl(req_id, actor_cast<abstract_actor*>(dh), std::forward<Ts>(xs)...);
    return req_id.response_id();
  }

  void request_sync_timeout_msg(const duration& dr, message_id mid);

  // returns 0 if last_dequeued() is an asynchronous or sync request message,
  // a response id generated from the request id otherwise
  inline message_id get_response_id() const {
    auto mid = current_element_->mid;
    return (mid.is_request()) ? mid.response_id() : message_id();
  }

  void forward_current_message(const actor& dest);

  void forward_current_message(const actor& dest, message_priority mp);

  template <class... Ts>
  void delegate(message_priority mp, const actor& dest, Ts&&... xs) {
    static_assert(sizeof...(Ts) > 0, "no message to send");
    if (! dest)
      return;
    auto mid = current_element_->mid;
    current_element_->mid = mp == message_priority::high
                             ? mid.with_high_priority()
                             : mid.with_normal_priority();
    current_element_->msg = make_message(std::forward<Ts>(xs)...);
    dest->enqueue(std::move(current_element_), context());
  }

  template <class... Ts>
  void delegate(const actor& dest, Ts&&... xs) {
    delegate(message_priority::normal, dest, std::forward<Ts>(xs)...);
  }

  template <class... Sigs, class... Ts>
  typename detail::deduce_output_type<
    detail::type_list<Sigs...>,
    detail::type_list<
      typename detail::implicit_conversions<
        typename std::decay<Ts>::type
      >::type...
    >
  >::delegated_type
  delegate(message_priority mp, const typed_actor<Sigs...>& dest, Ts&&... xs) {
    static_assert(sizeof...(Ts) > 0, "no message to send");
    using token =
      detail::type_list<
        typename detail::implicit_conversions<
          typename std::decay<Ts>::type
        >::type...>;
    token tk;
    check_typed_input(dest, tk);
    if (! dest)
      return {};
    auto mid = current_element_->mid;
    current_element_->mid = mp == message_priority::high
                             ? mid.with_high_priority()
                             : mid.with_normal_priority();
    current_element_->msg = make_message(std::forward<Ts>(xs)...);
    dest->enqueue(std::move(current_element_), context());
    return {};
  }

  template <class... Sigs, class... Ts>
  typename detail::deduce_output_type<
    detail::type_list<Sigs...>,
    detail::type_list<
      typename detail::implicit_conversions<
        typename std::decay<Ts>::type
      >::type...
    >
  >::delegated_type
  delegate(const typed_actor<Sigs...>& dest, Ts&&... xs) {
    return delegate(message_priority::normal, dest, std::forward<Ts>(xs)...);
  }

  inline exit_reason planned_exit_reason() const {
    return planned_exit_reason_;
  }

  inline void planned_exit_reason(exit_reason value) {
    planned_exit_reason_ = value;
  }

  inline detail::behavior_stack& bhvr_stack() {
    return bhvr_stack_;
  }

  inline mailbox_type& mailbox() {
    return mailbox_;
  }

  inline bool has_behavior() const {
    return ! bhvr_stack_.empty()
           || ! awaited_responses_.empty()
           || ! multiplexed_responses_.empty();
  }

  virtual void initialize() = 0;

  // clear behavior stack and call cleanup if actor either has no
  // valid behavior left or has set a planned exit reason
  bool finished();

  void cleanup(exit_reason reason, execution_unit* host) override;

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

  using error_handler = std::function<void (error&)>;

  using pending_response =
    std::pair<const message_id, std::pair<behavior, error_handler>>;

  message_id new_request_id(message_priority mp);

  void mark_awaited_arrived(message_id mid);

  bool awaits_response() const;

  bool awaits(message_id mid) const;

  maybe<pending_response&> find_awaited_response(message_id mid);

  void set_awaited_response_handler(message_id response_id, behavior bhvr,
                                    error_handler f = nullptr);

  behavior& awaited_response_handler();

  message_id awaited_response_id();

  void mark_multiplexed_arrived(message_id mid);

  bool multiplexes(message_id mid) const;

  maybe<pending_response&> find_multiplexed_response(message_id mid);

  void set_multiplexed_response_handler(message_id response_id, behavior bhvr,
                                        error_handler f = nullptr);

  // these functions are dispatched via the actor policies table

  void launch(execution_unit* eu, bool lazy, bool hide);

  using abstract_actor::enqueue;

  void enqueue(mailbox_element_ptr, execution_unit*) override;

  mailbox_element_ptr next_message();

  bool has_next_message();

  void push_to_cache(mailbox_element_ptr);

  bool invoke_from_cache();

  bool invoke_from_cache(behavior&, message_id);

  void do_become(behavior bhvr, bool discard_old);

protected:
  // used only in thread-mapped actors
  void await_data();

  // identifies the execution unit this actor is currently executed by
  execution_unit* context_;

  // identifies the ID of the last sent synchronous request
  message_id last_request_id_;

  // identifies all IDs of sync messages waiting for a response
  std::forward_list<pending_response> awaited_responses_;

  // identifies all IDs of async messages waiting for a response
  std::unordered_map<
    message_id,
    std::pair<behavior, error_handler>
  > multiplexed_responses_;

  // points to dummy_node_ if no callback is currently invoked,
  // points to the node under processing otherwise
  mailbox_element_ptr current_element_;

  // set by quit
  exit_reason planned_exit_reason_;

  // identifies the timeout messages we are currently waiting for
  uint32_t timeout_id_;

  // used by both event-based and blocking actors
  detail::behavior_stack bhvr_stack_;

  // used by both event-based and blocking actors
  mailbox_type mailbox_;

  // used by functor-based actors to implemented make_behavior() or act()
  std::function<behavior (local_actor*)> initial_behavior_fac_;

  // used for group management
  std::set<group> subscriptions_;

  /// @endcond

private:
  template <class T, class... Ts>
  typename std::enable_if<
    ! std::is_same<typename std::decay<T>::type, message>::value
  >::type
  send_impl(message_id mid, abstract_channel* dest, T&& x, Ts&&... xs) const {
    if (! dest)
      return;
    dest->enqueue(address(), mid,
                  make_message(std::forward<T>(x), std::forward<Ts>(xs)...),
                  context());
  }

  template <class T, class... Ts>
  typename std::enable_if<
    ! std::is_same<typename std::decay<T>::type, message>::value
  >::type
  send_impl(message_id mid, abstract_actor* dest, T&& x, Ts&&... xs) const {
    if (! dest)
      return;
    dest->enqueue(mailbox_element::make_joint(address(),
                                              mid, {},
                                              std::forward<T>(x),
                                              std::forward<Ts>(xs)...),
                  context());
  }

  void send_impl(message_id mid, abstract_channel* dest, message what) const;

  void delayed_send_impl(message_id mid, const channel& whom,
                         const duration& rtime, message data);
};

/// A smart pointer to a {@link local_actor} instance.
/// @relates local_actor
using local_actor_ptr = intrusive_ptr<local_actor>;

} // namespace caf

#endif // CAF_LOCAL_ACTOR_HPP
