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

#ifndef CAF_SCHEDULED_ACTOR_HPP
#define CAF_SCHEDULED_ACTOR_HPP

#include "caf/config.hpp"

#ifndef CAF_NO_EXCEPTIONS
#include <exception>
#endif // CAF_NO_EXCEPTIONS

#include <forward_list>
#include <map>
#include <type_traits>
#include <unordered_map>

#include "caf/actor_marker.hpp"
#include "caf/broadcast_scatterer.hpp"
#include "caf/error.hpp"
#include "caf/extend.hpp"
#include "caf/fwd.hpp"
#include "caf/inbound_path.hpp"
#include "caf/invoke_message_result.hpp"
#include "caf/local_actor.hpp"
#include "caf/logger.hpp"
#include "caf/no_stages.hpp"
#include "caf/output_stream.hpp"
#include "caf/response_handle.hpp"
#include "caf/scheduled_actor.hpp"
#include "caf/sec.hpp"
#include "caf/stream.hpp"
#include "caf/stream_manager.hpp"
#include "caf/stream_result.hpp"
#include "caf/stream_result_trait.hpp"
#include "caf/stream_source_trait.hpp"
#include "caf/to_string.hpp"

#include "caf/policy/arg.hpp"
#include "caf/policy/categorized.hpp"
#include "caf/policy/downstream_messages.hpp"
#include "caf/policy/normal_messages.hpp"
#include "caf/policy/upstream_messages.hpp"
#include "caf/policy/urgent_messages.hpp"

#include "caf/detail/behavior_stack.hpp"
#include "caf/detail/stream_sink_driver_impl.hpp"
#include "caf/detail/stream_sink_impl.hpp"
#include "caf/detail/stream_source_driver_impl.hpp"
#include "caf/detail/stream_source_impl.hpp"
#include "caf/detail/stream_stage_driver_impl.hpp"
#include "caf/detail/stream_stage_driver_impl.hpp"
#include "caf/detail/stream_stage_impl.hpp"
#include "caf/detail/tick_emitter.hpp"
#include "caf/detail/unordered_flat_map.hpp"

#include "caf/intrusive/drr_cached_queue.hpp"
#include "caf/intrusive/drr_queue.hpp"
#include "caf/intrusive/fifo_inbox.hpp"
#include "caf/intrusive/wdrr_dynamic_multiplexed_queue.hpp"
#include "caf/intrusive/wdrr_fixed_multiplexed_queue.hpp"

#include "caf/mixin/behavior_changer.hpp"
#include "caf/mixin/requester.hpp"
#include "caf/mixin/sender.hpp"

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

  /// Maps slot IDs to stream managers.
  using stream_manager_map = std::map<stream_slot, stream_manager_ptr>;

  /// Stores asynchronous messages with default priority.
  using default_queue = intrusive::drr_cached_queue<policy::normal_messages>;

  /// Stores asynchronous messages with hifh priority.
  using urgent_queue = intrusive::drr_cached_queue<policy::urgent_messages>;

  /// Stores upstream messages.
  using upstream_queue = intrusive::drr_queue<policy::upstream_messages>;

  /// Stores downstream messages.
  using downstream_queue =
    intrusive::wdrr_dynamic_multiplexed_queue<policy::downstream_messages>;

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

    using queue_type =
      intrusive::wdrr_fixed_multiplexed_queue<policy::categorized,
                                              default_queue, upstream_queue,
                                              downstream_queue, urgent_queue>;

    static constexpr size_t default_queue_index = 0;

    static constexpr size_t upstream_queue_index = 1;

    static constexpr size_t downstream_queue_index = 2;

    static constexpr size_t urgent_queue_index = 3;
  };

  /// A queue optimized for single-reader-many-writers.
  using mailbox_type = intrusive::fifo_inbox<mailbox_policy>;

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

  /// Consumes messages from the mailbox.
  struct mailbox_visitor {
    scheduled_actor* self;
    resume_result& result;
    size_t& handled_msgs;
    size_t max_throughput;

    /// Consumes upstream messages.
    intrusive::task_result operator()(size_t, upstream_queue&,
                                      mailbox_element&);

    /// Consumes downstream messages.
    intrusive::task_result
    operator()(size_t, downstream_queue&, stream_slot slot,
               policy::downstream_messages::nested_queue_type&,
               mailbox_element&);

    // Dispatches asynchronous messages with high and normal priority to the
    // same handler.
    template <class Queue>
    intrusive::task_result operator()(size_t, Queue&, mailbox_element& x) {
      return (*this)(x);
    }

    // Consumes asynchronous messages.
    intrusive::task_result operator()(mailbox_element& x);
  };

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

  /// Creates an output path for given source.
  template <class Out, class... Ts>
  output_stream<Out, Ts...>
  add_output_path(stream_source_ptr<Out, Ts...> ptr) {
    // The actual plumbing is done when returning the `output_stream<>` from a
    // message handler.
    return {0, std::move(ptr)};
  }

  /// Creates an output path for given stage.
  template <class In, class Out, class... Ts>
  output_stream<Out, Ts...>
  add_output_path(stream_stage_ptr<In, Out, Ts...> ptr) {
    // The actual plumbing is done when returning the `output_stream<>` from a
    // message handler.
    return {0, std::move(ptr)};
  }

  /// Creates an input path for given stage.
  template <class In, class Out, class... Ts>
  output_stream<Out, Ts...>
  add_input_path(const stream<In>&, stream_stage_ptr<In, Out, Ts...> mgr) {
    auto slot = next_slot();
    if (!add_stream_manager(slot, mgr)) {
      CAF_LOG_WARNING("unable to assign a manager to its slot");
      return {0, nullptr};
    }
    return {slot, std::move(mgr)};
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
  template <class Driver,
            class Scatterer = broadcast_scatterer<typename Driver::output_type>,
            class... Ts>
  typename Driver::output_stream_type make_source(Ts&&... xs) {
    auto ptr = detail::make_stream_source<Driver, Scatterer>(
      this, std::forward<Ts>(xs)...);
    return {0, std::move(ptr)};
  }

  template <class... Ts, class Init, class Pull, class Done,
            class Scatterer =
              broadcast_scatterer<typename stream_source_trait_t<Pull>::output>,
            class Trait = stream_source_trait_t<Pull>>
  output_stream<typename Trait::output, detail::decay_t<Ts>...>
  make_source(std::tuple<Ts...> xs, Init init, Pull pull, Done done,
              policy::arg<Scatterer> = {}) {
    using tuple_type = std::tuple<detail::decay_t<Ts>...>;
    using driver = detail::stream_source_driver_impl<typename Trait::output,
                                                     Pull, Done, tuple_type>;
    return make_source<driver, Scatterer>(std::move(init), std::move(pull),
                                          std::move(done), std::move(xs));
  }

  template <class... Ts, class Init, class Pull, class Done,
            class Scatterer =
              broadcast_scatterer<typename stream_source_trait_t<Pull>::output>,
            class Trait = stream_source_trait_t<Pull>>
  output_stream<typename Trait::output, detail::decay_t<Ts>...>
  make_source(Init init, Pull pull, Done done,
              policy::arg<Scatterer> scatterer_type = {}) {
    return make_source(std::make_tuple(), init, pull, done, scatterer_type);
  }

  template <class Driver, class Input, class... Ts>
  stream_result<typename Driver::output_type> make_sink(const stream<Input>&,
                                                        Ts&&... xs) {
    auto slot = next_slot();
    auto ptr = detail::make_stream_sink<Driver>(this, std::forward<Ts>(xs)...);
    if (!add_stream_manager(slot, ptr)) {
      CAF_LOG_WARNING("unable to add a stream manager for a sink");
      return {0, nullptr};
    }
    return {slot, std::move(ptr)};
  }

  template <class Input, class Init, class Fun, class Finalize,
            class Trait = stream_sink_trait_t<Fun, Finalize>>
  stream_result<typename Trait::output>
  make_sink(const stream<Input>& in, Init init, Fun fun, Finalize fin) {
    using driver = detail::stream_sink_driver_impl<typename Trait::input,
                                                   Fun, Finalize>;
    return make_sink<driver>(in, std::move(init), std::move(fun),
                             std::move(fin));
  }

  template <class Driver,
            class Scatterer = broadcast_scatterer<typename Driver::output_type>,
            class Input = int, class... Ts>
  typename Driver::output_stream_type make_stage(const stream<Input>&,
                                                 Ts&&... xs) {
    auto slot = next_slot();
    auto ptr = detail::make_stream_stage<Driver, Scatterer>(
      this, std::forward<Ts>(xs)...);
    if (!add_stream_manager(slot, ptr)) {
      CAF_LOG_WARNING("unable to add a stream manager for a stage");
      return {0, nullptr};
    }
    return {slot, std::move(ptr)};
  }

  template <class In, class... Ts, class Init, class Fun, class Cleanup,
            class Scatterer =
              broadcast_scatterer<typename stream_stage_trait_t<Fun>::output>,
            class Trait = stream_stage_trait_t<Fun>>
  output_stream<typename Trait::output, detail::decay_t<Ts>...>
  make_stage(const stream<In>& in, std::tuple<Ts...> xs, Init init, Fun fun,
             Cleanup cleanup, policy::arg<Scatterer> scatterer_type = {}) {
    CAF_IGNORE_UNUSED(scatterer_type);
    CAF_ASSERT(current_mailbox_element() != nullptr);
    CAF_ASSERT(
      current_mailbox_element()->content().match_elements<open_stream_msg>());
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
    using driver =
      detail::stream_stage_driver_impl<typename Trait::input, output_type, Fun,
                                       Cleanup,
                                       std::tuple<detail::decay_t<Ts>...>>;
    return make_stage<driver, Scatterer>(in, std::move(init), std::move(fun),
                                         std::move(cleanup), std::move(xs));
  }

  /// Returns a stream manager (implementing a continuous stage) without in- or
  /// outbound path. The returned manager is not connected to any slot and thus
  /// not stored by the actor automatically.
  template <class Driver,
            class Scatterer = broadcast_scatterer<typename Driver::output_type>,
            class Input = int, class... Ts>
  typename Driver::stage_ptr_type make_continuous_stage(Ts&&... xs) {
    auto ptr = detail::make_stream_stage<Driver, Scatterer>(
      this, std::forward<Ts>(xs)...);
    ptr->continuous(true);
    return std::move(ptr);
  }

  template <class... Ts, class Init, class Fun, class Cleanup,
            class Scatterer =
              broadcast_scatterer<typename stream_stage_trait_t<Fun>::output>,
            class Trait = stream_stage_trait_t<Fun>>
  stream_stage_ptr<typename Trait::input, typename Trait::output,
                   detail::decay_t<Ts>...>
  make_continuous_stage(std::tuple<Ts...> xs, Init init, Fun fun,
                        Cleanup cleanup,
                        policy::arg<Scatterer> scatterer_type = {}) {
    CAF_IGNORE_UNUSED(scatterer_type);
    using input_type = typename Trait::input;
    using output_type = typename Trait::output;
    using state_type = typename Trait::state;
    static_assert(std::is_same<
                    void (state_type&),
                    typename detail::get_callable_trait<Init>::fun_sig
                  >::value,
                  "Expected signature `void (State&)` for init function");
    static_assert(std::is_same<
                    void (state_type&, downstream<output_type>&, input_type),
                    typename detail::get_callable_trait<Fun>::fun_sig
                  >::value,
                  "Expected signature `void (State&, downstream<Out>&, In)` "
                  "for consume function");
    using driver =
      detail::stream_stage_driver_impl<typename Trait::input, output_type, Fun,
                                       Cleanup,
                                       std::tuple<detail::decay_t<Ts>...>>;
    return make_continuous_stage<driver, Scatterer>(
      std::move(init), std::move(fun), std::move(cleanup), std::move(xs));
  }

  template <class Init, class Fun, class Cleanup,
            class Scatterer =
              broadcast_scatterer<typename stream_stage_trait_t<Fun>::output>,
            class Trait = stream_stage_trait_t<Fun>>
  stream_stage_ptr<typename stream_stage_trait_t<Fun>::input,
                   typename stream_stage_trait_t<Fun>::output>
  make_continuous_stage(Init init, Fun fun, Cleanup cleanup,
                        policy::arg<Scatterer> scatterer_type = {}) {
    return make_continuous_stage(std::make_tuple(), std::move(init),
                                std::move(fun), std::move(cleanup),
                                scatterer_type);
  }

  /*
    /// Creates a new stream source and starts streaming to `dest`.
    /// @param dest Actor handle to the stream destination.
    /// @param xs User-defined handshake payload.
    /// @param init Function object for initializing the state of the source.
    /// @param getter Function object for generating messages for the stream.
    /// @param pred Predicate returning `true` when the stream is done.
    /// @param res_handler Function object for receiving the stream result.
    /// @param scatterer_type Configures the policy for downstream
    communication.
    /// @returns A stream object with a pointer to the generated
    `stream_manager`. template <class Handle, class... Ts, class Init, class
    Getter, class ClosedPredicate, class ResHandler, class Scatterer =
    broadcast_scatterer< typename stream_source_trait_t<Getter>::output>>
    output_stream<typename stream_source_trait_t<Getter>::output, Ts...>
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
                    "Expected signature `void (State&, downstream<T>&, size_t)`
    " "for getter function"); static_assert(std::is_same< bool (const
    state_type&), typename detail::get_callable_trait< ClosedPredicate
                      >::fun_sig
                    >::value,
                    "Expected signature `bool (const State&)` for "
                    "closed_predicate function");
      if (!dest) {
        CAF_LOG_ERROR("cannot stream to an invalid actor handle");
        return none;
      }
      // Generate new stream ID and manager
      auto slot = next_slot();
      auto ptr = detail::make_stream_source(this, std::move(init),
                                            std::move(getter), std::move(pred),
                                            scatterer_type);
      outbound_path::emit_open(this, slot, actor_cast<strong_actor_ptr>(dest),
                               make_handshake<type>(std::move(xs)),
                               stream_priority::normal, false);
      ptr->generate_messages();
      pending_stream_managers_.emplace(slot, ptr);
      return {slot, std::move(ptr)};
    }

    /// Creates a new stream source and starts streaming to `dest`.
    /// @param dest Actor handle to the stream destination.
    /// @param init Function object for initializing the state of the source.
    /// @param getter Function object for generating messages for the stream.
    /// @param pred Predicate returning `true` when the stream is done.
    /// @param res_handler Function object for receiving the stream result.
    /// @param scatterer_type Configures the policy for downstream
    communication.
    /// @returns A stream object with a pointer to the generated
    `stream_manager`. template <class Handle, class Init, class Getter, class
    ClosedPredicate, class ResHandler, class Scatterer = broadcast_scatterer<
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
    /// @param scatterer_type Configures the policy for downstream
    communication.
    /// @returns A stream object with a pointer to the generated
    `stream_manager`. template <class Init, class... Ts, class Getter, class
    ClosedPredicate, class Scatterer = broadcast_scatterer< typename
    stream_source_trait_t<Getter>::output>> output_stream<typename
    stream_source_trait_t<Getter>::output, Ts...> make_source(std::tuple<Ts...>
    xs, Init init, Getter getter, ClosedPredicate pred, policy::arg<Scatterer>
    scatterer_type = {}) { CAF_IGNORE_UNUSED(scatterer_type); using type =
    typename stream_source_trait_t<Getter>::output; using state_type = typename
    stream_source_trait_t<Getter>::state; static_assert(std::is_same< void
    (state_type&), typename detail::get_callable_trait<Init>::fun_sig
                    >::value,
                    "Expected signature `void (State&)` for init function");
      static_assert(std::is_same<
                      void (state_type&, downstream<type>&, size_t),
                      typename detail::get_callable_trait<Getter>::fun_sig
                    >::value,
                    "Expected signature `void (State&, downstream<T>&, size_t)`
    " "for getter function"); static_assert(std::is_same< bool (const
    state_type&), typename detail::get_callable_trait< ClosedPredicate
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
    /// @param scatterer_type Configures the policy for downstream
    communication.
    /// @returns A stream object with a pointer to the generated
    `stream_manager`. template <class Init, class Getter, class ClosedPredicate,
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
    /// @returns A stream object with a pointer to the generated
    `stream_manager`. template <class In, class... Ts, class Init, class Fun,
    class Cleanup, class Gatherer = random_gatherer, class Scatterer =
                broadcast_scatterer<typename stream_stage_trait_t<Fun>::output>>
    output_stream<typename stream_stage_trait_t<Fun>::output, Ts...>
    make_stage(const stream<In>& in, std::tuple<Ts...> xs, Init init, Fun fun,
               Cleanup cleanup, policy::arg<Gatherer, Scatterer> policies = {})
    { CAF_IGNORE_UNUSED(policies); CAF_ASSERT(current_mailbox_element() !=
    nullptr);
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
    /// @returns A stream object with a pointer to the generated
    `stream_manager`. template <class In, class Init, class Fun, class Cleanup,
              class Gatherer = random_gatherer,
              class Scatterer =
                broadcast_scatterer<typename stream_stage_trait_t<Fun>::output>>
    stream<typename stream_stage_trait_t<Fun>::output>
    make_stage(const stream<In>& in, Init init, Fun fun, Cleanup cleanup,
               policy::arg<Gatherer, Scatterer> policies = {}) {
      return make_stage(in, std::make_tuple(), std::move(init), std::move(fun),
                        std::move(cleanup), policies);
    }

    /// Creates a new stream sink of type T.
    /// @pre `current_mailbox_element()` is a `stream_msg::open` handshake
    /// @param in The input of the sink.
    /// @param f Callback for initializing the object after successful creation.
    /// @param xs Parameter pack for creating the instance of T.
    /// @returns A stream object with a pointer to the generated
    `stream_manager`. template <class T, class In, class SuccessCallback,
    class... Ts> stream_result<typename T::output_type> make_sink_impl(const
    stream<In>& in, SuccessCallback f, Ts&&... xs) {
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
                      std::move(opn.original_stage), opn.priority, rp)) {
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
    /// @returns A stream object with a pointer to the generated
    `stream_manager`. template <class In, class Init, class Fun, class Finalize,
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

  */
  /// @cond PRIVATE

  /// Builds the pipeline after receiving an `open_stream_msg`. Sends a stream
  /// handshake to the next actor if `mgr->out().terminal() == false`,
  /// otherwise
  /// @private
  sec build_pipeline(stream_manager_ptr mgr);


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
  virtual invoke_message_result consume(mailbox_element& x);

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

  /// Returns whether `true` if the behavior stack is not empty or
  /// if outstanding responses exist, `false` otherwise.
  inline bool has_behavior() const {
    return !bhvr_stack_.empty()
           || !awaited_responses_.empty()
           || !multiplexed_responses_.empty();
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

/*
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
  /// @param result_cb Callback for the listener of the final stream result.
  ///                  Ignored when returning `nullptr`, because the previous
  ///                  stage is responsible for it until this manager
  ///                  acknowledges the handshake.
  /// @returns `true` if the sink could be added to the manager, `false`
  ///          otherwise.
  bool add_source(const stream_manager_ptr& mgr, const stream_id& sid,
                  strong_actor_ptr source_ptr, strong_actor_ptr original_stage,
                  stream_priority prio, response_promise result_cb);

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
                    none)) {
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
                    mptr->sender, ignore_mid ? make_message_id() : mptr->mid,
                    std::move(stages),
                    make<stream_msg::open>(
                      sid, address(), make_message_from_tuple(std::move(ys)),
                      ctrl(), next, stream_priority::normal, false)),
                  context());
    if (!ignore_mid)
      mptr->mid.mark_as_answered();
  }
*/

  void push_to_cache(mailbox_element_ptr ptr);

  /// @endcond

  /// Returns the queue for storing incoming messages.
  inline mailbox_type& mailbox() {
    return mailbox_;
  }

  /// @private
  default_queue& get_default_queue();

  /// @private
  upstream_queue& get_upstream_queue();

  /// @private
  downstream_queue& get_downstream_queue();

  /// @private
  urgent_queue& get_urgent_queue();

  // -- inbound_path management ------------------------------------------------

  inbound_path* make_inbound_path(stream_manager_ptr mgr, stream_slots slots,
                                  strong_actor_ptr sender) override;

  void erase_inbound_path_later(stream_slot slot) override;

  void erase_inbound_paths_later(const stream_manager* mgr) override;

  void erase_inbound_paths_later(const stream_manager* mgr,
                                 error reason) override;

  // -- handling of stream message ---------------------------------------------

  void handle_upstream_msg(stream_slots slots, actor_addr& sender,
                           upstream_msg::ack_open& x);

  template <class T>
  void handle_upstream_msg(stream_slots slots, actor_addr&, T& x) {
    auto i = stream_managers_.find(slots.receiver);
    if (i == stream_managers_.end()) {
      auto j = pending_stream_managers_.find(slots.receiver);
      if (j != pending_stream_managers_.end()) {
        j->second->abort(sec::stream_init_failed);
        pending_stream_managers_.erase(j);
        return;
      }
      CAF_LOG_INFO("no manager found:" << CAF_ARG(slots));
      // TODO: replace with `if constexpr` when switching to C++17
      if (std::is_same<T, upstream_msg::ack_batch>::value) {
        // Make sure the other actor does not falsely believe us a source.
        inbound_path::emit_irregular_shutdown(this, slots, current_sender(),
                                              sec::invalid_upstream);
      }
      return;
    }
    CAF_ASSERT(i->second != nullptr);
    i->second->handle(slots, x);
    if (i->second->done()) {
      CAF_LOG_INFO("done sending:" << CAF_ARG(slots));
      i->second->stop();
      stream_managers_.erase(i);
      if (stream_managers_.empty())
        stream_ticks_.stop();
    }
  }

  /// @cond PRIVATE

  // -- utility functions for invoking default handler -------------------------

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

  // -- timeout management -----------------------------------------------------

  /// Requests a new timeout and returns its ID.
  uint64_t set_timeout(atom_value type, actor_clock::time_point x);

  // -- stream processing ------------------------------------------------------

  /// Returns a currently unused slot.
  stream_slot next_slot();

  /// Adds a new stream manager to the actor and starts cycle management if
  /// needed.
  bool add_stream_manager(stream_slot id, stream_manager_ptr ptr);

  /// Removes the stream manager mapped to `id` in `O(log n)`.
  void erase_stream_manager(stream_slot id);

  /// Removes all entries for `mgr` in `O(n)`.
  void erase_stream_manager(const stream_manager_ptr& mgr);

  /// @pre `x.content().match_elements<open_stream_msg>()`
  invoke_message_result handle_open_stream_msg(mailbox_element& x);

  /// Advances credit and batch timeouts and returns the timestamp when to call
  /// this function again.
  actor_clock::time_point advance_streams(actor_clock::time_point now);

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

  /// Customization point for setting a default `exit_msg` callback.
  exit_handler exit_handler_;

  /// Stores stream managers for established streams.
  stream_manager_map stream_managers_;

  /// Stores stream managers for pending streams, i.e., streams that have not
  /// yet received an ACK.
  stream_manager_map pending_stream_managers_;

  /// Controls batch and credit timeouts.
  detail::tick_emitter stream_ticks_;

  /// Number of ticks per batch delay.
  long max_batch_delay_ticks_;

  /// Number of ticks of each credit round.
  long credit_round_ticks_;

  /// Pointer to a private thread object associated with a detached actor.
  detail::private_thread* private_thread_;

# ifndef CAF_NO_EXCEPTIONS
  /// Customization point for setting a default exception callback.
  exception_handler exception_handler_;
# endif // CAF_NO_EXCEPTIONS

  /// @endcond
};

} // namespace caf

#endif // CAF_SCHEDULED_ACTOR_HPP
