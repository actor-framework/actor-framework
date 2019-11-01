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

#include <type_traits>

#include "caf/actor_traits.hpp"
#include "caf/catch_all.hpp"
#include "caf/message_id.hpp"
#include "caf/none.hpp"
#include "caf/sec.hpp"
#include "caf/system_messages.hpp"
#include "caf/typed_behavior.hpp"

#include "caf/detail/type_list.hpp"
#include "caf/detail/typed_actor_util.hpp"

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

  template <class T = traits, class F = none_t, class OnError = none_t,
            class = detail::enable_if_t<T::is_non_blocking>>
  void await(F f, OnError g) const {
    static_assert(detail::is_callable<F>::value, "F must provide a single, "
                                                 "non-template operator()");
    static_assert(detail::is_callable<F>::value,
                  "OnError must provide a single, non-template operator() "
                  "that takes a caf::error");
    using result_type = typename detail::get_callable_trait<F>::result_type;
    static_assert(std::is_same<void, result_type>::value,
                  "response handlers are not allowed to have a return "
                  "type other than void");
    detail::type_checker<response_type, F>::check();
    policy_.await(self_, std::move(f), std::move(g));
  }

  template <class T = traits, class F = none_t,
            class = detail::enable_if_t<T::is_non_blocking>>
  void await(F f) const {
    auto self = self_;
    await(std::move(f), [self](error& err) { self->call_error_handler(err); });
  }

  template <class T = traits, class F = none_t, class OnError = none_t,
            class = detail::enable_if_t<T::is_non_blocking>>
  void then(F f, OnError g) const {
    static_assert(detail::is_callable<F>::value, "F must provide a single, "
                                                 "non-template operator()");
    static_assert(detail::is_callable<F>::value,
                  "OnError must provide a single, non-template operator() "
                  "that takes a caf::error");
    using result_type = typename detail::get_callable_trait<F>::result_type;
    static_assert(std::is_same<void, result_type>::value,
                  "response handlers are not allowed to have a return "
                  "type other than void");
    detail::type_checker<response_type, F>::check();
    policy_.then(self_, std::move(f), std::move(g));
  }

  template <class T = traits, class F = none_t,
            class = detail::enable_if_t<T::is_non_blocking>>
  void then(F f) const {
    auto self = self_;
    then(std::move(f), [self](error& err) { self->call_error_handler(err); });
  }

  // -- blocking API -----------------------------------------------------------

  template <class T = traits, class F = none_t, class OnError = none_t,
            class = detail::enable_if_t<T::is_blocking>>
  detail::is_handler_for_ef<OnError, error> receive(F f, OnError g) {
    static_assert(detail::is_callable<F>::value, "F must provide a single, "
                                                 "non-template operator()");
    static_assert(detail::is_callable<F>::value,
                  "OnError must provide a single, non-template operator() "
                  "that takes a caf::error");
    using result_type = typename detail::get_callable_trait<F>::result_type;
    static_assert(std::is_same<void, result_type>::value,
                  "response handlers are not allowed to have a return "
                  "type other than void");
    detail::type_checker<response_type, F>::check();
    policy_.receive(self_, std::move(f), std::move(g));
  }

  template <class T = traits, class OnError = none_t, class F = none_t,
            class = detail::enable_if_t<T::is_blocking>>
  detail::is_handler_for_ef<OnError, error> receive(OnError g, F f) {
    // TODO: allowing blocking actors to pass the error handler in first may be
    //       more flexible, but it makes the API asymmetric. Consider
    //       deprecating / removing this member function.
    receive(std::move(f), std::move(g));
  }

  template <class T = policy_type, class OnError = none_t, class F = none_t,
            class E = detail::is_handler_for_ef<OnError, error>,
            class = detail::enable_if_t<T::is_trivial>>
  void receive(OnError g, catch_all<F> f) {
    // TODO: this bypasses the policy. Either we deprecate `catch_all` or *all*
    //       policies must support it. Currently, we only enable this member
    //       function on the trivial policy for backwards compatibility.
    typename actor_type::accept_one_cond rc;
    self_->varargs_receive(rc, id(), std::move(g), std::move(f));
  }

  // -- properties -------------------------------------------------------------

  template <class T = policy_type, class = detail::enable_if_t<T::is_trivial>>
  message_id id() const noexcept {
    return policy_.id();
  }

  actor_type* self() noexcept {
    return self_;
  }

private:
  /// Points to the parent actor.
  actor_type* self_;

  /// Configures how the actor wants to process an incoming response.
  policy_type policy_;
};

} // namespace caf
