// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/actor_traits.hpp"
#include "caf/catch_all.hpp"
#include "caf/detail/callable_trait.hpp"
#include "caf/disposable.hpp"
#include "caf/flow/fwd.hpp"
#include "caf/fwd.hpp"
#include "caf/message_id.hpp"
#include "caf/type_list.hpp"

#include <type_traits>

namespace caf::detail {

/// Holds state for a event-based response handles.
template <class ActorType>
struct simple_response_handle_state {
  static constexpr bool is_fan_out = false;

  /// Points to the parent actor.
  ActorType* self;

  /// Stores the IDs of the messages we are waiting for.
  message_id mid;

  /// Stores a handle to the in-flight timeout.
  disposable pending_timeout;

  template <class Policy>
  simple_response_handle_state(ActorType* selfptr, const Policy& policy)
    : self(selfptr),
      mid(policy.id()),
      pending_timeout(policy.pending_timeouts()) {
    // nop
  }
};

template <class ActorType>
struct fan_out_response_handle_state {
  static constexpr bool is_fan_out = true;

  /// Points to the parent actor.
  ActorType* self;

  /// Stores the IDs of the messages we are waiting for.
  std::vector<message_id> mids;

  /// Stores a handle to the in-flight timeout.
  disposable pending_timeout;

  template <class Policy>
  fan_out_response_handle_state(ActorType* selfptr, const Policy& policy)
    : self(selfptr),
      mids(policy.ids()),
      pending_timeout(policy.pending_timeouts()) {
    // nop
  }
};

template <bool IsFanOut, class ActorType>
struct select_response_handle_state;

template <class ActorType>
struct select_response_handle_state<true, ActorType> {
  using type = fan_out_response_handle_state<ActorType>;
};

template <class ActorType>
struct select_response_handle_state<false, ActorType> {
  using type = simple_response_handle_state<ActorType>;
};

} // namespace caf::detail

namespace caf {

/// This helper class identifies an expected response message and enables
/// `request(...).then(...)`.
template <class ActorType, class Policy>
class response_handle {
public:
  // -- member types -----------------------------------------------------------

  using actor_type = ActorType;

  using traits = actor_traits<actor_type>;

  using policy_type = Policy;

  using response_type = typename policy_type::response_type;

  using state_type =
    typename detail::select_response_handle_state<!policy_type::is_trivial,
                                                  actor_type>::type;

  // -- constructors, destructors, and assignment operators --------------------

  response_handle() = delete;

  response_handle(const response_handle&) = delete;

  response_handle& operator=(const response_handle&) = delete;

  response_handle(response_handle&&) noexcept = default;

  response_handle& operator=(response_handle&&) noexcept = delete;

  template <class... Ts>
  explicit response_handle(actor_type* self, Ts&&... xs)
    : self_(self), policy_(std::forward<Ts>(xs)...) {
    // nop
  }

  // -- non-blocking API -------------------------------------------------------

  template <detail::callable F, std::invocable<error&> OnError>
    requires traits::is_non_blocking
  void await(F f, OnError g) {
    using result_type = typename detail::get_callable_trait<F>::result_type;
    static_assert(std::is_same_v<void, result_type>,
                  "response handlers are not allowed to have a return "
                  "type other than void");
    policy_type::template type_checker<F>::check();
    policy_.await(self_, std::move(f), std::move(g));
  }

  template <class F>
    requires traits::is_non_blocking
  void await(F f) {
    await(std::move(f),
          [self = self_](error& err) { self->call_error_handler(err); });
  }

  template <detail::callable F, std::invocable<error&> OnError>
    requires traits::is_non_blocking
  void then(F f, OnError g) {
    using result_type = typename detail::get_callable_trait<F>::result_type;
    static_assert(std::is_same_v<void, result_type>,
                  "response handlers are not allowed to have a return "
                  "type other than void");
    policy_type::template type_checker<F>::check();
    policy_.then(self_, std::move(f), std::move(g));
  }

  template <class F>
    requires traits::is_non_blocking
  void then(F f) {
    auto self = self_;
    then(std::move(f), [self](error& err) { self->call_error_handler(err); });
  }

  template <class T>
  auto as_single() && {
    state_type state{self_, policy_};
    return self_->response_to_single(type_list_v<T>, state);
  }

  template <class T>
  auto as_observable() && {
    state_type state{self_, policy_};
    return self_->response_to_observable(type_list_v<T>, state,
                                         typename Policy::tag_type{});
  }

  // -- blocking API -----------------------------------------------------------

  template <class F, class OnError>
    requires(traits::is_blocking && detail::handler_for<OnError, error>)
  void receive(F f, OnError g) {
    static_assert(detail::callable<F>,
                  "F must provide a single, non-template operator()");
    static_assert(std::is_invocable_v<OnError, error&>,
                  "OnError must provide an operator() that takes a caf::error");
    using result_type = typename detail::get_callable_trait<F>::result_type;
    static_assert(std::is_same_v<void, result_type>,
                  "response handlers are not allowed to have a return "
                  "type other than void");
    policy_type::template type_checker<F>::check();
    policy_.receive(self_, std::move(f), std::move(g));
  }

  template <class OnError, class F>
    requires(traits::is_blocking && detail::handler_for<OnError, error>)
  void receive(OnError g, F f) {
    // TODO: allowing blocking actors to pass the error handler in first may be
    //       more flexible, but it makes the API asymmetric. Consider
    //       deprecating / removing this member function.
    receive(std::move(f), std::move(g));
  }

  template <class OnError, class F>
    requires(policy_type::is_trivial && detail::handler_for<OnError, error>)
  void receive(OnError g, catch_all<F> f) {
    // TODO: this bypasses the policy. Either we deprecate `catch_all` or *all*
    //       policies must support it. Currently, we only enable this member
    //       function on the trivial policy for backwards compatibility.
    typename actor_type::accept_one_cond rc;
    self_->varargs_receive(rc, id(), std::move(g), std::move(f));
  }

  // -- properties -------------------------------------------------------------

  message_id id() const noexcept
    requires policy_type::is_trivial
  {
    return policy_.id();
  }

  actor_type* self() noexcept {
    return self_;
  }

  policy_type& policy() noexcept {
    return policy_;
  }

private:
  /// Points to the parent actor.
  actor_type* self_;

  /// Configures how the actor wants to process an incoming response.
  policy_type policy_;
};

} // namespace caf
