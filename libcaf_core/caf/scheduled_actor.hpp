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
#include "caf/no_stages.hpp"
#include "caf/local_actor.hpp"
#include "caf/actor_marker.hpp"
#include "caf/stream_result.hpp"
#include "caf/response_handle.hpp"
#include "caf/scheduled_actor.hpp"
#include "caf/random_gatherer.hpp"
#include "caf/stream_sink_impl.hpp"
#include "caf/stream_stage_impl.hpp"
#include "caf/stream_source_impl.hpp"
#include "caf/stream_result_trait.hpp"
#include "caf/broadcast_scatterer.hpp"
#include "caf/terminal_stream_scatterer.hpp"

#include "caf/to_string.hpp"

#include "caf/policy/arg.hpp"

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

  /// A reference-counting pointer to a `stream_manager`.
  using stream_manager_ptr = intrusive_ptr<stream_manager>;

  /// A container for associating stream IDs to handlers.
  using streams_map = std::unordered_map<stream_id, stream_manager_ptr>;

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

  /// Returns a new stream ID.
  stream_id make_stream_id() {
    return {ctrl(), new_request_id(message_priority::normal).integer_value()};
  }

  /// Creates a new stream source and starts streaming to `dest`.
  /// @param dest Actor handle to the stream destination.
  /// @param xs User-defined handshake payload.
  /// @param init Function object for initializing the state of the source.
  /// @param getter Function object for generating messages for the stream.
  /// @param pred Predicate returning `true` when the stream is done.
  /// @param res_handler Function object for receiving the stream result.
  /// @param scatterer_type Configures the policy for downstream communication.
  /// @returns A stream object with a pointer to the generated `stream_manager`.
  template <class Handle, class... Ts, class Init, class Getter,
            class ClosedPredicate, class ResHandler,
            class Scatterer = broadcast_scatterer<
              typename stream_source_trait_t<Getter>::output>>
  annotated_stream<typename stream_source_trait_t<Getter>::output, Ts...>
  make_source(const Handle& dest, std::tuple<Ts...> xs, Init init,
              Getter getter, ClosedPredicate pred, ResHandler res_handler,
              policy::arg<Scatterer> scatterer_type = {}) {
    CAF_IGNORE_UNUSED(scatterer_type);
    using type = typename stream_source_trait_t<Getter>::output;
    using state_type = typename stream_source_trait_t<Getter>::state;
    static_assert(std::is_same<
                    void (state_type&),
                    typename detail::get_callable_trait<Init>::fun_sig
                  >::value,
                  "Expected signature `void (State&)` for init function");
    static_assert(std::is_same<
                    void (state_type&, downstream<type>&, size_t),
                    typename detail::get_callable_trait<Getter>::fun_sig
                  >::value,
                  "Expected signature `void (State&, downstream<T>&, size_t)` "
                  "for getter function");
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
      return none;
    }
    // generate new stream ID and manager
    auto sid = make_stream_id();
    using impl = stream_source_impl<Getter, ClosedPredicate, Scatterer>;
    auto ptr = make_counted<impl>(this, std::move(getter), std::move(pred));
    auto mid = new_request_id(message_priority::normal);
    if (!add_sink<type>(ptr, sid, ctrl(), actor_cast<strong_actor_ptr>(dest),
                        no_stages, mid, stream_priority::normal, std::move(xs)))
      return none;
    init(ptr->state());
    this->add_multiplexed_response_handler(
      mid.response_id(),
      stream_result_trait_t<ResHandler>::make_result_handler(res_handler));
    streams_.emplace(sid, ptr);
    return {std::move(sid), std::move(ptr)};
  }

  /// Creates a new stream source and starts streaming to `dest`.
  /// @param dest Actor handle to the stream destination.
  /// @param init Function object for initializing the state of the source.
  /// @param getter Function object for generating messages for the stream.
  /// @param pred Predicate returning `true` when the stream is done.
  /// @param res_handler Function object for receiving the stream result.
  /// @param scatterer_type Configures the policy for downstream communication.
  /// @returns A stream object with a pointer to the generated `stream_manager`.
  template <class Handle, class Init, class Getter, class ClosedPredicate,
            class ResHandler,
            class Scatterer = broadcast_scatterer<
              typename stream_source_trait_t<Getter>::output>>
  stream<typename stream_source_trait_t<Getter>::output>
  make_source(const Handle& dest, Init init, Getter getter,
              ClosedPredicate pred, ResHandler res_handler,
              policy::arg<Scatterer> scatterer_type = {}) {
    return make_source(dest, std::make_tuple(), std::move(init),
                       std::move(getter), std::move(pred),
                       std::move(res_handler), scatterer_type);
  }

  /// Creates a new stream source.
  /// @param xs User-defined handshake payload.
  /// @param init Function object for initializing the state of the source.
  /// @param getter Function object for generating messages for the stream.
  /// @param pred Predicate returning `true` when the stream is done.
  /// @param scatterer_type Configures the policy for downstream communication.
  /// @returns A stream object with a pointer to the generated `stream_manager`.
  template <class Init, class... Ts, class Getter, class ClosedPredicate,
            class Scatterer = broadcast_scatterer<
              typename stream_source_trait_t<Getter>::output>>
  annotated_stream<typename stream_source_trait_t<Getter>::output, Ts...>
  make_source(std::tuple<Ts...> xs, Init init, Getter getter,
              ClosedPredicate pred,
              policy::arg<Scatterer> scatterer_type = {}) {
    CAF_IGNORE_UNUSED(scatterer_type);
    using type = typename stream_source_trait_t<Getter>::output;
    using state_type = typename stream_source_trait_t<Getter>::state;
    static_assert(std::is_same<
                    void (state_type&),
                    typename detail::get_callable_trait<Init>::fun_sig
                  >::value,
                  "Expected signature `void (State&)` for init function");
    static_assert(std::is_same<
                    void (state_type&, downstream<type>&, size_t),
                    typename detail::get_callable_trait<Getter>::fun_sig
                  >::value,
                  "Expected signature `void (State&, downstream<T>&, size_t)` "
                  "for getter function");
    static_assert(std::is_same<
                    bool (const state_type&),
                    typename detail::get_callable_trait<
                      ClosedPredicate
                    >::fun_sig
                  >::value,
                  "Expected signature `bool (const State&)` for "
                  "closed_predicate function");
    auto sid = make_stream_id();
    using impl = stream_source_impl<Getter, ClosedPredicate, Scatterer>;
    auto ptr = make_counted<impl>(this, std::move(getter), std::move(pred));
    auto next = take_current_next_stage();
    if (!add_sink<type>(ptr, sid, current_sender(), std::move(next),
                        take_current_forwarding_stack(), current_message_id(),
                        stream_priority::normal, std::move(xs))) {
      CAF_LOG_ERROR("cannot create stream source without sink");
      auto rp = make_response_promise();
      rp.deliver(sec::no_downstream_stages_defined);
      return none;
    }
    drop_current_message_id();
    init(ptr->state());
    streams_.emplace(sid, ptr);
    return {std::move(sid), std::move(ptr)};
  }

  /// Creates a new stream source.
  /// @param init Function object for initializing the state of the source.
  /// @param getter Function object for generating messages for the stream.
  /// @param pred Predicate returning `true` when the stream is done.
  /// @param scatterer_type Configures the policy for downstream communication.
  /// @returns A stream object with a pointer to the generated `stream_manager`.
  template <class Init, class Getter, class ClosedPredicate,
            class Scatterer = broadcast_scatterer<
              typename stream_source_trait_t<Getter>::output>>
  stream<typename stream_source_trait_t<Getter>::output>
  make_source(Init init, Getter getter, ClosedPredicate pred,
              policy::arg<Scatterer> scatterer_type = {}) {
    return make_source(std::make_tuple(), std::move(init), std::move(getter),
                       std::move(pred), scatterer_type);
  }

  /// Creates a new stream stage.
  /// @pre `current_mailbox_element()` is a `stream_msg::open` handshake
  /// @param in The input of the stage.
  /// @param xs User-defined handshake payload.
  /// @param init Function object for initializing the state of the stage.
  /// @param fun Function object for processing stream elements.
  /// @param cleanup Function object for clearing the stage of the stage.
  /// @param policies Sets the policies for up- and downstream communication.
  /// @returns A stream object with a pointer to the generated `stream_manager`.
  template <class In, class... Ts, class Init, class Fun, class Cleanup,
            class Gatherer = random_gatherer,
            class Scatterer =
              broadcast_scatterer<typename stream_stage_trait_t<Fun>::output>>
  annotated_stream<typename stream_stage_trait_t<Fun>::output, Ts...>
  make_stage(const stream<In>& in, std::tuple<Ts...> xs, Init init, Fun fun,
             Cleanup cleanup, policy::arg<Gatherer, Scatterer> policies = {}) {
    CAF_IGNORE_UNUSED(policies);
    CAF_ASSERT(current_mailbox_element() != nullptr);
    CAF_ASSERT(current_mailbox_element()->content().match_elements<stream_msg>());
    using output_type = typename stream_stage_trait_t<Fun>::output;
    using state_type = typename stream_stage_trait_t<Fun>::state;
    static_assert(std::is_same<
                    void (state_type&),
                    typename detail::get_callable_trait<Init>::fun_sig
                  >::value,
                  "Expected signature `void (State&)` for init function");
    static_assert(std::is_same<
                    void (state_type&, downstream<output_type>&, In),
                    typename detail::get_callable_trait<Fun>::fun_sig
                  >::value,
                  "Expected signature `void (State&, downstream<Out>&, In)` "
                  "for consume function");
    using impl = stream_stage_impl<Fun, Cleanup, Gatherer, Scatterer>;
    auto ptr = make_counted<impl>(this, in.id(), std::move(fun),
                                  std::move(cleanup));
    if (!serve_as_stage<output_type>(ptr, in, std::move(xs))) {
      CAF_LOG_ERROR("installing sink and source to the manager failed");
      return none;
    }
    init(ptr->state());
    return {in.id(), std::move(ptr)};
  }

  /// Creates a new stream stage.
  /// @pre `current_mailbox_element()` is a `stream_msg::open` handshake
  /// @param in The input of the stage.
  /// @param init Function object for initializing the state of the stage.
  /// @param fun Function object for processing stream elements.
  /// @param cleanup Function object for clearing the stage of the stage.
  /// @param policies Sets the policies for up- and downstream communication.
  /// @returns A stream object with a pointer to the generated `stream_manager`.
  template <class In, class Init, class Fun, class Cleanup,
            class Gatherer = random_gatherer,
            class Scatterer =
              broadcast_scatterer<typename stream_stage_trait_t<Fun>::output>>
  stream<typename stream_stage_trait_t<Fun>::output>
  make_stage(const stream<In>& in, Init init, Fun fun, Cleanup cleanup,
             policy::arg<Gatherer, Scatterer> policies = {}) {
    return add_stage(in, std::make_tuple(), std::move(init), std::move(fun),
                     std::move(cleanup), policies);
  }

  /// Creates a new stream sink of type T.
  /// @pre `current_mailbox_element()` is a `stream_msg::open` handshake
  /// @param in The input of the sink.
  /// @param f Callback for initializing the object after successful creation.
  /// @param xs Parameter pack for creating the instance of T.
  /// @returns A stream object with a pointer to the generated `stream_manager`.
  template <class T, class In, class SuccessCallback, class... Ts>
  stream_result<typename T::output_type>
  make_sink_impl(const stream<In>& in, SuccessCallback f, Ts&&... xs) {
    CAF_ASSERT(current_mailbox_element() != nullptr);
    CAF_ASSERT(current_mailbox_element()->content().match_elements<stream_msg>());
    auto& sm = current_mailbox_element()->content().get_as<stream_msg>(0);
    CAF_ASSERT(holds_alternative<stream_msg::open>(sm.content));
    auto& opn = get<stream_msg::open>(sm.content);
    auto sid = in.id();
    auto next = take_current_next_stage();
    auto ptr = make_counted<T>(this, std::forward<Ts>(xs)...);
    auto rp = make_response_promise();
    if (!add_source(ptr, sid, std::move(opn.prev_stage),
                    std::move(opn.original_stage), opn.priority,
                    opn.redeployable, rp)) {
      CAF_LOG_ERROR("cannot create stream stage without source");
      rp.deliver(sec::cannot_add_upstream);
      return none;
    }
    f(*ptr);
    streams_.emplace(in.id(), ptr);
    return {in.id(), std::move(ptr)};
  }

  /// Creates a new stream sink.
  /// @pre `current_mailbox_element()` is a `stream_msg::open` handshake
  /// @param in The input of the sink.
  /// @param init Function object for initializing the state of the stage.
  /// @param fun Function object for processing stream elements.
  /// @param finalize Function object for producing the final result.
  /// @param policies Sets the policies for up- and downstream communication.
  /// @returns A stream object with a pointer to the generated `stream_manager`.
  template <class In, class Init, class Fun, class Finalize,
            class Gatherer = random_gatherer,
            class Scatterer = terminal_stream_scatterer>
  stream_result<typename stream_sink_trait_t<Fun, Finalize>::output>
  make_sink(const stream<In>& in, Init init, Fun fun, Finalize finalize,
            policy::arg<Gatherer, Scatterer> policies = {}) {
    CAF_IGNORE_UNUSED(policies);
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
    using impl = stream_sink_impl<Fun, Finalize, Gatherer, Scatterer>;
    auto initializer = [&](impl& x) {
      init(x.state());
    };
    return make_sink_impl<impl>(in, initializer, std::move(fun),
                                std::move(finalize));
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
  static message make_handshake(const stream_id& sid, std::tuple<Ts...>& xs) {
    stream<T> token{sid};
    auto ys = std::tuple_cat(std::forward_as_tuple(token), std::move(xs));
    return make_message_from_tuple(std::move(ys));
  }

  /// Tries to add a new sink to the stream manager `mgr`.
  /// @param mgr Pointer to the responsible stream manager.
  /// @param sid The ID used for communicating to the sink.
  /// @param origin Handle to the actor that initiated the stream and that will
  ///               receive the stream result (if any).
  /// @param sink_ptr Handle to the new sink.
  /// @param fwd_stack Forwarding stack for the remaining stream participants.
  /// @param prio Priority of the traffic to the sink.
  /// @param handshake_mid Message ID for the stream handshake. If valid, this
  ///                      ID will be used to send the result to the `origin`.
  /// @param data Additional payload for the stream handshake.
  /// @returns `true` if the sink could be added to the manager, `false`
  ///          otherwise.
  template <class T, class... Ts>
  bool add_sink(const stream_manager_ptr& mgr, const stream_id& sid,
                strong_actor_ptr origin, strong_actor_ptr sink_ptr,
                mailbox_element::forwarding_stack fwd_stack,
                message_id handshake_mid, stream_priority prio,
                std::tuple<Ts...> data) {
    CAF_ASSERT(mgr != nullptr);
    if (!sink_ptr || !sid.valid())
      return false;
    return mgr->add_sink(sid, std::move(origin), std::move(sink_ptr),
                         std::move(fwd_stack), handshake_mid,
                         make_handshake<T>(sid, data), prio, false);
  }

  /// Tries to add a new source to the stream manager `mgr`.
  /// @param mgr Pointer to the responsible stream manager.
  /// @param sid The ID used for communicating to the sink.
  /// @param source_ptr Handle to the new source.
  /// @param prio Priority of the traffic from the source.
  /// @param redeployable Configures whether source can re-appear after aborts.
  /// @param result_cb Callback for the listener of the final stream result.
  ///                  Ignored when returning `nullptr`, because the previous
  ///                  stage is responsible for it until this manager
  ///                  acknowledges the handshake.
  /// @returns `true` if the sink could be added to the manager, `false`
  ///          otherwise.
  bool add_source(const stream_manager_ptr& mgr, const stream_id& sid,
                  strong_actor_ptr source_ptr, strong_actor_ptr original_stage,
                  stream_priority prio, bool redeployable,
                  response_promise result_cb);

  /// Adds a new source to the stream manager `mgr` if `current_message()` is a
  /// `stream_msg::open` handshake.
  /// @param mgr Pointer to the responsible stream manager.
  /// @param sid The ID used for communicating to the sink.
  /// @param result_cb Callback for the listener of the final stream result.
  ///                  Ignored when returning `nullptr`, because the previous
  ///                  stage is responsible for it until this manager
  ///                  acknowledges the handshake.
  /// @returns `true` if the sink could be added to the manager, `false`
  ///          otherwise.
  bool add_source(const stream_manager_ptr& mgr, const stream_id& sid,
                  response_promise result_cb);

  /// Adds a pair of source and sink to `mgr` that allows it to serve as a
  /// stage for the stream `in` with the output type `Out`. Returns `false` if
  /// `current_mailbox_element()` is not a `stream_msg::open` handshake or if
  /// adding the sink or source fails, `true` otherwise.
  /// @param mgr Pointer to the responsible stream manager.
  /// @param in The input of the stage.
  /// @param xs User-defined handshake payload.
  /// @returns `true` if the manager added sink and source, `false` otherwise.
  template <class Out, class In, class... Ts>
  bool serve_as_stage(const stream_manager_ptr& mgr, const stream<In>& in,
                      std::tuple<Ts...> xs) {
    CAF_ASSERT(current_mailbox_element() != nullptr);
    if (!current_mailbox_element()->content().match_elements<stream_msg>())
      return false;
    auto& sm = current_mailbox_element()->content().get_as<stream_msg>(0);
    if (!holds_alternative<stream_msg::open>(sm.content))
      return false;
    auto& opn = get<stream_msg::open>(sm.content);
    auto sid = in.id();
    auto next = take_current_next_stage();
    if (!add_sink<Out>(mgr, sid, current_sender(), std::move(next),
                       current_forwarding_stack(), current_message_id(),
                       opn.priority, std::move(xs))) {
      CAF_LOG_ERROR("cannot create stream stage without sink");
      mgr->abort(sec::no_downstream_stages_defined);
      auto rp = make_response_promise();
      rp.deliver(sec::no_downstream_stages_defined);
      return false;
    }
    // Pass `none` instead of a response promise to the source, because the
    // outbound_path is responsible for the "promise" until we receive an ACK
    // from the next stage.
    if (!add_source(mgr, sid, std::move(opn.prev_stage),
                    std::move(opn.original_stage), opn.priority,
                    opn.redeployable, none)) {
      CAF_LOG_ERROR("cannot create stream stage without source");
      auto rp = make_response_promise();
      rp.deliver(sec::cannot_add_upstream);
      return false;
    }
    streams_.emplace(sid, mgr);
    return true;
  }


  template <class T, class... Ts>
  void fwd_stream_handshake(const stream_id& sid, std::tuple<Ts...>& xs,
                            bool ignore_mid = false) {
    auto mptr = current_mailbox_element();
    auto& stages = mptr->stages;
    CAF_ASSERT(!stages.empty());
    CAF_ASSERT(stages.back() != nullptr);
    auto next = std::move(stages.back());
    stages.pop_back();
    stream<T> token{sid};
    auto ys = std::tuple_cat(std::forward_as_tuple(token), std::move(xs));;
    next->enqueue(make_mailbox_element(
                    mptr->sender, ignore_mid ? message_id::make() : mptr->mid,
                    std::move(stages),
                    make<stream_msg::open>(
                      sid, address(), make_message_from_tuple(std::move(ys)),
                      ctrl(), next, stream_priority::normal, false)),
                  context());
    if (!ignore_mid)
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

  /// Stores the home execution unit.
  execution_unit* home_eu_;

  /// @endcond
};

} // namespace caf

#endif // CAF_ABSTRACT_EVENT_BASED_ACTOR_HPP
