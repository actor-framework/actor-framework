/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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

#ifndef CAF_ABSTRACT_EVENT_BASED_ACTOR_HPP
#define CAF_ABSTRACT_EVENT_BASED_ACTOR_HPP

#include "caf/config.hpp"

#ifndef CAF_NO_EXCEPTIONS
#include <exception>
#endif // CAF_NO_EXCEPTIONS

#include <type_traits>

#include "caf/fwd.hpp"
#include "caf/sec.hpp"
#include "caf/error.hpp"
#include "caf/extend.hpp"
#include "caf/local_actor.hpp"
#include "caf/actor_marker.hpp"
#include "caf/stream_result.hpp"
#include "caf/response_handle.hpp"
#include "caf/scheduled_actor.hpp"
#include "caf/stream_sink_impl.hpp"
#include "caf/stream_stage_impl.hpp"
#include "caf/stream_source_impl.hpp"
#include "caf/stream_result_trait.hpp"

#include "caf/to_string.hpp"

#include "caf/policy/greedy.hpp"
#include "caf/policy/anycast.hpp"
#include "caf/policy/broadcast.hpp"

#include "caf/mixin/sender.hpp"
#include "caf/mixin/requester.hpp"
#include "caf/mixin/behavior_changer.hpp"

#include "caf/logger.hpp"

namespace caf {

// -- related free functions ---------------------------------------------------

/// @relates scheduled_actor
/// Default handler function that sends the message back to the sender.
result<message> reflect(scheduled_actor*, message_view&);

/// @relates scheduled_actor
/// Default handler function that sends
/// the message back to the sender and then quits.
result<message> reflect_and_quit(scheduled_actor*, message_view&);

/// @relates scheduled_actor
/// Default handler function that prints messages
/// message via `aout` and drops them afterwards.
result<message> print_and_drop(scheduled_actor*, message_view&);

/// @relates scheduled_actor
/// Default handler function that simply drops messages.
result<message> drop(scheduled_actor*, message_view&);

/// A cooperatively scheduled, event-based actor implementation. This is the
/// recommended base class for user-defined actors.
/// @extends local_actor
class scheduled_actor : public local_actor, public resumable {
public:
  // -- member types -----------------------------------------------------------

  /// A reference-counting pointer to a `stream_handler`.
  using stream_handler_ptr = intrusive_ptr<stream_handler>;

  /// A container for associating stream IDs to handlers.
  using streams_map = std::unordered_map<stream_id, stream_handler_ptr>;

  /// The message ID of an outstanding response with its callback.
  using pending_response = std::pair<const message_id, behavior>;

  /// A pointer to a scheduled actor.
  using pointer = scheduled_actor*;

  /// Function object for handling unmatched messages.
  using default_handler =
    std::function<result<message>(pointer, message_view&)>;

  /// Function object for handling error messages.
  using error_handler = std::function<void (pointer, error&)>;

  /// Function object for handling down messages.
  using down_handler = std::function<void (pointer, down_msg&)>;

  /// Function object for handling exit messages.
  using exit_handler = std::function<void (pointer, exit_msg&)>;

# ifndef CAF_NO_EXCEPTIONS
  /// Function object for handling exit messages.
  using exception_handler = std::function<error (pointer, std::exception_ptr&)>;
# endif // CAF_NO_EXCEPTIONS

  // -- nested enums -----------------------------------------------------------

  /// @cond PRIVATE

  /// Categorizes incoming messages.
  enum class message_category {
    /// Denotes an expired and thus obsolete timeout.
    expired_timeout,
    /// Triggers the currently active timeout.
    timeout,
    /// Triggers the current behavior.
    ordinary,
    /// Triggers handlers for system messages such as `exit_msg` or `down_msg`.
    internal
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

  /// @endcond

  // -- static helper functions ------------------------------------------------

  static void default_error_handler(pointer ptr, error& x);

  static void default_down_handler(pointer ptr, down_msg& x);

  static void default_exit_handler(pointer ptr, exit_msg& x);

# ifndef CAF_NO_EXCEPTIONS
  static error default_exception_handler(pointer ptr, std::exception_ptr& x);
# endif // CAF_NO_EXCEPTIONS

  // -- constructors and destructors -------------------------------------------

  explicit scheduled_actor(actor_config& cfg);

  ~scheduled_actor() override;

  // -- overridden functions of abstract_actor ---------------------------------

  using abstract_actor::enqueue;

  void enqueue(mailbox_element_ptr ptr, execution_unit* eu) override;

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
  void quit(error x = error{});

  // -- event handlers ---------------------------------------------------------

  /// Sets a custom handler for unexpected messages.
  inline void set_default_handler(default_handler fun) {
    if (fun)
      default_handler_ = std::move(fun);
    else
      default_handler_ = print_and_drop;
  }

  /// Sets a custom handler for unexpected messages.
  template <class F>
  typename std::enable_if<
    std::is_convertible<
      F,
      std::function<result<message> (type_erased_tuple&)>
    >::value
  >::type
  set_default_handler(F fun) {
    default_handler_ = [=](scheduled_actor*, const type_erased_tuple& xs) {
      return fun(xs);
    };
  }

  /// Sets a custom handler for error messages.
  inline void set_error_handler(error_handler fun) {
    if (fun)
      error_handler_ = std::move(fun);
    else
      error_handler_ = default_error_handler;
  }

  /// Sets a custom handler for error messages.
  template <class T>
  auto set_error_handler(T fun) -> decltype(fun(std::declval<error&>())) {
    set_error_handler([fun](scheduled_actor*, error& x) { fun(x); });
  }

  /// Sets a custom handler for down messages.
  inline void set_down_handler(down_handler fun) {
    if (fun)
      down_handler_ = std::move(fun);
    else
      down_handler_ = default_down_handler;
  }

  /// Sets a custom handler for down messages.
  template <class T>
  auto set_down_handler(T fun) -> decltype(fun(std::declval<down_msg&>())) {
    set_down_handler([fun](scheduled_actor*, down_msg& x) { fun(x); });
  }

  /// Sets a custom handler for error messages.
  inline void set_exit_handler(exit_handler fun) {
    if (fun)
      exit_handler_ = std::move(fun);
    else
      exit_handler_ = default_exit_handler;
  }

  /// Sets a custom handler for exit messages.
  template <class T>
  auto set_exit_handler(T fun) -> decltype(fun(std::declval<exit_msg&>())) {
    set_exit_handler([fun](scheduled_actor*, exit_msg& x) { fun(x); });
  }

# ifndef CAF_NO_EXCEPTIONS
  /// Sets a custom exception handler for this actor. If multiple handlers are
  /// defined, only the functor that was added *last* is being executed.
  inline void set_exception_handler(exception_handler fun) {
    if (fun)
      exception_handler_ = std::move(fun);
    else
      exception_handler_ = default_exception_handler;
  }

  /// Sets a custom exception handler for this actor. If multiple handlers are
  /// defined, only the functor that was added *last* is being executed.
  template <class F>
  typename std::enable_if<
    std::is_convertible<
      F,
      std::function<error (std::exception_ptr&)>
    >::value
  >::type
  set_exception_handler(F f) {
    set_exception_handler([f](scheduled_actor*, std::exception_ptr& x) {
      return f(x);
    });
  }
# endif // CAF_NO_EXCEPTIONS

  // -- stream management ------------------------------------------------------

  /// Owning poiner to a `downstream_policy`.
  using downstream_policy_ptr = std::unique_ptr<downstream_policy>;

  /// Owning poiner to an `upstream_policy`.
  using upstream_policy_ptr = std::unique_ptr<upstream_policy>;

  // Starts a new stream.
  template <class Handle, class... Ts, class Init, class Getter,
            class ClosedPredicate, class ResHandler>
  annotated_stream<typename stream_source_trait_t<Getter>::output, Ts...>
  new_stream(const Handle& dest, std::tuple<Ts...> xs, Init init, Getter getter,
             ClosedPredicate pred, ResHandler res_handler,
             downstream_policy_ptr dpolicy = nullptr) {
    using type = typename stream_source_trait_t<Getter>::output;
    using state_type = typename stream_source_trait_t<Getter>::state;
    static_assert(std::is_same<
                    void (state_type&),
                    typename detail::get_callable_trait<Init>::fun_sig
                  >::value,
                  "Expected signature `void (State&)` for init function");
    static_assert(std::is_same<
                    bool (const state_type&),
                    typename detail::get_callable_trait<
                      ClosedPredicate
                    >::fun_sig
                  >::value,
                  "Expected signature `bool (const State&)` for "
                  "closed_predicate function");
    if (!dest) {
      CAF_LOG_ERROR("cannot stream to an invalid actor handle");
      return {stream_id{nullptr, 0}, nullptr};
    }
    // generate new stream ID
    stream_id sid{ctrl(),
                  new_request_id(message_priority::normal).integer_value()};
    stream<type> token{sid};
    auto ys = std::tuple_cat(std::forward_as_tuple(token), std::move(xs));;
    // generate new ID for the final response message and send handshake
    auto res_id = new_request_id(message_priority::normal);
    dest->enqueue(
      make_mailbox_element(
        ctrl(), res_id, {},
        make<stream_msg::open>(sid, make_message_from_tuple(std::move(ys)),
                               ctrl(), actor_cast<strong_actor_ptr>(dest),
                               stream_priority::normal, false)),
      context());
    // install response handler
    this->add_multiplexed_response_handler(
      res_id.response_id(),
      stream_result_trait_t<ResHandler>::make_result_handler(res_handler));
    // install stream handler
    using impl = stream_source_impl<Getter, ClosedPredicate>;
    if (dpolicy == nullptr)
      dpolicy.reset(new policy::anycast);
    auto ptr = make_counted<impl>(this, sid, std::move(dpolicy),
                                  std::move(getter), std::move(pred));
    init(ptr->state());
    // Add downstream with 0 credit.
    // TODO: add multiple downstreams when receiving a composed actor handle
    auto next = actor_cast<strong_actor_ptr>(dest);
    ptr->add_downstream(next);
    streams_.emplace(sid, ptr);
    return {std::move(sid), std::move(ptr)};
  }

  // Starts a new stream.
  template <class Handle, class Init, class Getter,
            class ClosedPredicate, class ResHandler>
  stream<typename stream_source_trait_t<Getter>::output>
  new_stream(const Handle& dest, Init init, Getter getter,
             ClosedPredicate pred, ResHandler res_handler) {
    return new_stream(dest, std::make_tuple(), std::move(init),
                      std::move(getter), std::move(pred),
                      std::move(res_handler));
  }

  /// Adds a stream source to this actor.
  template <class Init, class... Ts, class Getter, class ClosedPredicate>
  annotated_stream<typename stream_source_trait_t<Getter>::output, Ts...>
  add_source(std::tuple<Ts...> xs, Init init, Getter getter,
             ClosedPredicate pred, downstream_policy_ptr dpolicy = nullptr) {
    CAF_ASSERT(current_mailbox_element() != nullptr);
    using type = typename stream_source_trait_t<Getter>::output;
    using state_type = typename stream_source_trait_t<Getter>::state;
    static_assert(std::is_same<
                    void (state_type&),
                    typename detail::get_callable_trait<Init>::fun_sig
                  >::value,
                  "Expected signature `void (State&)` for init function");
    static_assert(std::is_same<
                    bool (const state_type&),
                    typename detail::get_callable_trait<
                      ClosedPredicate
                    >::fun_sig
                  >::value,
                  "Expected signature `bool (const State&)` for "
                  "closed_predicate function");
    auto& stages = current_mailbox_element()->stages;
    if (stages.empty()) {
      CAF_LOG_ERROR("cannot create a stream data source without downstream");
      auto rp = make_response_promise();
      rp.deliver(sec::no_downstream_stages_defined);
      return {stream_id{nullptr, 0}, nullptr};
    }
    stream_id sid{ctrl(),
                  new_request_id(message_priority::normal).integer_value()};
    auto next = stages.back();
    CAF_ASSERT(next != nullptr);
    fwd_stream_handshake<type>(sid, xs);
    using impl = stream_source_impl<Getter, ClosedPredicate>;
    if (dpolicy == nullptr)
      dpolicy.reset(new policy::anycast);
    auto ptr = make_counted<impl>(this, sid, std::move(dpolicy),
                                  std::move(getter), std::move(pred));
    init(ptr->state());
    streams_.emplace(sid, ptr);
    // Add downstream with 0 credit.
    // TODO: add multiple downstreams when receiving a composed actor handle
    ptr->add_downstream(next);
    return {std::move(sid), std::move(ptr)};
  }

  template <class Init, class Getter, class ClosedPredicate>
  stream<typename stream_source_trait_t<Getter>::output>
  add_source(Init init, Getter getter, ClosedPredicate pred) {
    return add_source(std::make_tuple(), std::move(init),
                      std::move(getter), std::move(pred));
  }

  /// Adds a stream stage to this actor.
  template <class In, class... Ts, class Init, class Fun, class Cleanup>
  annotated_stream<typename stream_stage_trait_t<Fun>::output, Ts...>
  add_stage(const stream<In>& in, std::tuple<Ts...> xs, Init init, Fun fun,
            Cleanup cleanup_fun, upstream_policy_ptr upolicy = nullptr,
            downstream_policy_ptr dpolicy = nullptr) {
    CAF_ASSERT(current_mailbox_element() != nullptr);
    using output_type = typename stream_stage_trait_t<Fun>::output;
    using state_type = typename stream_stage_trait_t<Fun>::state;
    static_assert(std::is_same<
                    void (state_type&),
                    typename detail::get_callable_trait<Init>::fun_sig
                  >::value,
                  "Expected signature `void (State&)` for init function");
    auto& stages = current_mailbox_element()->stages;
    if (stages.empty()) {
      CAF_LOG_ERROR("cannot create a stream data source without downstream");
      return stream_id{nullptr, 0};
    }
    auto sid = in.id();
    auto next = stages.back();
    CAF_ASSERT(next != nullptr);
    fwd_stream_handshake<output_type>(sid, xs);
    using impl = stream_stage_impl<Fun, Cleanup>;
    if (upolicy == nullptr)
      upolicy.reset(new policy::greedy);
    if (dpolicy == nullptr)
      dpolicy.reset(new policy::anycast);
    auto ptr = make_counted<impl>(this, sid, std::move(upolicy),
                                  std::move(dpolicy), std::move(fun),
                                  std::move(cleanup_fun));
    init(ptr->state());
    streams_.emplace(sid, ptr);
    // Add downstream with 0 credit.
    // TODO: add multiple downstreams when receiving a composed actor handle
    ptr->add_downstream(next);
    return {std::move(sid), std::move(ptr)};
  }

  /// Adds a stream stage to this actor.
  template <class In, class Init, class Fun, class Cleanup>
  stream<typename stream_stage_trait_t<Fun>::output>
  add_stage(const stream<In>& in, Init init, Fun fun, Cleanup cleanup_fun) {
    return add_stage(in, std::make_tuple(), std::move(init),
                     std::move(fun), std::move(cleanup_fun));
  }

  /// Adds a stream sink to this actor.
  template <class In, class Init, class Fun, class Finalize>
  stream_result<typename stream_sink_trait_t<Fun, Finalize>::output>
  add_sink(const stream<In>& in, Init init, Fun fun, Finalize finalize_fun,
           upstream_policy_ptr upolicy = nullptr) {
    CAF_ASSERT(current_mailbox_element() != nullptr);
    //using output_type = typename stream_sink_trait_t<Fun, Finalize>::output;
    using state_type = typename stream_sink_trait_t<Fun, Finalize>::state;
    static_assert(std::is_same<
                    void (state_type&),
                    typename detail::get_callable_trait<Init>::fun_sig
                  >::value,
                  "Expected signature `void (State&)` for init function");
    static_assert(std::is_same<
                    void (state_type&, In),
                    typename detail::get_callable_trait<Fun>::fun_sig
                  >::value,
                  "Expected signature `void (State&, Input)` "
                  "for consume function");
    auto mptr = current_mailbox_element();
    if (!mptr) {
      CAF_LOG_ERROR("add_sink called outside of a message handler");
      return {stream_id{nullptr, 0}, nullptr};
    }
    using impl = stream_sink_impl<Fun, Finalize>;
    if (upolicy == nullptr)
      upolicy.reset(new policy::greedy);
    auto ptr = make_counted<impl>(this, std::move(upolicy),
                                  std::move(mptr->sender),
                                  std::move(mptr->stages), mptr->mid,
                                  std::move(fun), std::move(finalize_fun));
    init(ptr->state());
    streams_.emplace(in.id(), ptr);
    return {in.id(), std::move(ptr)};
  }

  inline streams_map& streams() {
    return streams_;
  }

  /// Tries to send more data on all downstream paths. Use this function to
  /// manually trigger batches in a source after receiving more data to send.
  void trigger_downstreams();

  /// @cond PRIVATE

  // -- timeout management -----------------------------------------------------

  /// Requests a new timeout and returns its ID.
  uint32_t request_timeout(const duration& d);

  /// Resets the timeout if `timeout_id` is the active timeout.
  void reset_timeout(uint32_t timeout_id);

  /// Returns whether `timeout_id` is currently active.
  bool is_active_timeout(uint32_t tid) const;

  // -- message processing -----------------------------------------------------

  /// Adds a callback for an awaited response.
  void add_awaited_response_handler(message_id response_id, behavior bhvr);

  /// Adds a callback for a multiplexed response.
  void add_multiplexed_response_handler(message_id response_id, behavior bhvr);

  /// Returns the category of `x`.
  message_category categorize(mailbox_element& x);

  /// Tries to consume `x`.
  virtual invoke_message_result consume(mailbox_element& x);

  /// Tries to consume `x`.
  void consume(mailbox_element_ptr x);

  /// Tries to consume one element form the cache using the current behavior.
  bool consume_from_cache();

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

  /// Returns whether `true` if the behavior stack is not empty or
  /// if outstanding responses exist, `false` otherwise.
  inline bool has_behavior() const {
    return !bhvr_stack_.empty()
           || !awaited_responses_.empty()
           || !multiplexed_responses_.empty()
           || !streams_.empty();
  }

  inline behavior& current_behavior() {
    return !awaited_responses_.empty() ? awaited_responses_.front().second
                                        : bhvr_stack_.back();
  }

  /// Installs a new behavior without performing any type checks.
  void do_become(behavior bhvr, bool discard_old);

  /// Performs cleanup code for the actor if it has no active
  /// behavior or was explicitly terminated.
  /// @returns `true` if cleanup code was called, `false` otherwise.
  bool finalize();

  inline detail::behavior_stack& bhvr_stack() {
    return bhvr_stack_;
  }

  template <class T, class... Ts>
  void fwd_stream_handshake(const stream_id& sid, std::tuple<Ts...>& xs) {
    auto mptr = current_mailbox_element();
    auto& stages = mptr->stages;
    CAF_ASSERT(!stages.empty());
    CAF_ASSERT(stages.back() != nullptr);
    auto next = std::move(stages.back());
    stages.pop_back();
    stream<T> token{sid};
    auto ys = std::tuple_cat(std::forward_as_tuple(token), std::move(xs));;
    next->enqueue(
      make_mailbox_element(
        mptr->sender, mptr->mid, std::move(stages),
        make<stream_msg::open>(sid, make_message_from_tuple(std::move(ys)),
                               ctrl(), next, stream_priority::normal, false)),
      context());
    mptr->mid.mark_as_answered();
  }

  /// @endcond

protected:
  /// @cond PRIVATE

  /// Utility function that swaps `f` into a temporary before calling it
  /// and restoring `f` only if it has not been replaced by the user.
  template <class F, class... Ts>
  auto call_handler(F& f, Ts&&... xs)
  -> typename std::enable_if<
       !std::is_same<decltype(f(std::forward<Ts>(xs)...)), void>::value,
       decltype(f(std::forward<Ts>(xs)...))
     >::type {
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
  -> typename std::enable_if<
       std::is_same<decltype(f(std::forward<Ts>(xs)...)), void>::value
     >::type {
    using std::swap;
    F g;
    swap(f, g);
    g(std::forward<Ts>(xs)...);
    if (!f)
      swap(g, f);
  }

  bool handle_stream_msg(mailbox_element& x, behavior* active_behavior);

  // -- Member Variables -------------------------------------------------------

  /// Stores user-defined callbacks for message handling.
  detail::behavior_stack bhvr_stack_;

  /// Identifies the timeout messages we are currently waiting for.
  uint32_t timeout_id_;

  /// Stores callbacks for awaited responses.
  std::forward_list<pending_response> awaited_responses_;

  /// Stores callbacks for multiplexed responses.
  std::unordered_map<message_id, behavior> multiplexed_responses_;

  /// Customization point for setting a default `message` callback.
  default_handler default_handler_;

  /// Customization point for setting a default `error` callback.
  error_handler error_handler_;

  /// Customization point for setting a default `down_msg` callback.
  down_handler down_handler_;

  /// Customization point for setting a default `exit_msg` callback.
  exit_handler exit_handler_;

  /// Pointer to a private thread object associated with a detached actor.
  detail::private_thread* private_thread_;

  // TODO: this type is quite heavy in terms of memory, maybe use vector?
  /// Holds state for all streams running through this actor.
  streams_map streams_;

# ifndef CAF_NO_EXCEPTIONS
  /// Customization point for setting a default exception callback.
  exception_handler exception_handler_;
# endif // CAF_NO_EXCEPTIONS

  /// @endcond
};

} // namespace caf

#endif // CAF_ABSTRACT_EVENT_BASED_ACTOR_HPP
