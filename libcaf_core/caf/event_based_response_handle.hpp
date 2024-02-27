// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/disposable.hpp"
#include "caf/flow/fwd.hpp"
#include "caf/message_id.hpp"
#include "caf/scheduled_actor.hpp"
#include "caf/type_list.hpp"

#include <type_traits>

namespace caf::detail {

template <class T>
struct event_based_response_handle_res {
  using type = T;
};

template <class T>
struct event_based_response_handle_res<type_list<T>> {
  using type = T;
};

template <class T0, class T1, class... Ts>
struct event_based_response_handle_res<type_list<T0, T1, Ts...>> {};

template <>
struct event_based_response_handle_res<message> {};

template <class T>
using event_based_response_handle_res_t =
  typename event_based_response_handle_res<T>::type;

} // namespace caf::detail

namespace caf {

/// This helper class identifies an expected response message and enables
/// `request(...).then(...)`.
template <class Result>
class event_based_response_handle {
public:
  // -- constructors, destructors, and assignment operators --------------------

  event_based_response_handle(scheduled_actor* self, message_id mid,
                              disposable pending_timeout)
    : self_(self), mid_(mid), pending_timeout_(std::move(pending_timeout)) {
    // nop
  }

  // -- then and await ---------------------------------------------------------

  template <class OnValue, class OnError>
  void await(OnValue on_value, OnError on_error) && {
    type_check<OnValue, OnError>();
    auto bhvr = behavior{std::move(on_value), std::move(on_error)};
    self_->add_awaited_response_handler(mid_, std::move(bhvr),
                                        std::move(pending_timeout_));
  }

  template <class OnValue>
  void await(OnValue on_value) && {
    return std::move(*this).await(std::move(on_value),
                                  [self = self_](error& err) {
                                    self->call_error_handler(err);
                                  });
  }

  template <class OnValue, class OnError>
  void then(OnValue on_value, OnError on_error) && {
    type_check<OnValue, OnError>();
    auto bhvr = behavior{std::move(on_value), std::move(on_error)};
    self_->add_multiplexed_response_handler(mid_, std::move(bhvr),
                                            std::move(pending_timeout_));
  }

  template <class OnValue>
  void then(OnValue on_value) && {
    return std::move(*this).then(std::move(on_value),
                                 [self = self_](error& err) {
                                   self->call_error_handler(err);
                                 });
  }

  // -- conversions ------------------------------------------------------------

  template <class T = Result, class Self = scheduled_actor>
  flow::assert_scheduled_actor_hdr_t<
    flow::observable<detail::event_based_response_handle_res_t<T>>>
  as_observable() && {
    using res_t = detail::event_based_response_handle_res_t<T>;
    return static_cast<Self*>(self_)
      ->template single_from_response<res_t>(mid_, std::move(pending_timeout_))
      .as_observable();
  }

private:
  template <class OnValue, class OnError>
  static constexpr void type_check() {
    // Type-check OnValue.
    using on_value_trait_helper = typename detail::get_callable_trait<OnValue>;
    static_assert(on_value_trait_helper::valid,
                  "OnValue must provide a single, non-template operator()");
    using on_value_trait = typename on_value_trait_helper::type;
    static_assert(std::is_same_v<typename on_value_trait::result_type, void>,
                  "OnValue must return void");
    if constexpr (std::is_same_v<Result, type_list<void>>) {
      using on_value_args = typename on_value_trait::decayed_arg_types;
      static_assert(std::is_same_v<on_value_args, type_list<>>,
                    "OnValue does not match the expected response types");
    } else if constexpr (!std::is_same_v<Result, message>) {
      using on_value_args = typename on_value_trait::decayed_arg_types;
      static_assert(std::is_same_v<on_value_args, Result>,
                    "OnValue does not match the expected response types");
    }
    // Type-check OnError.
    using on_error_trait_helper = typename detail::get_callable_trait<OnError>;
    static_assert(on_error_trait_helper::valid,
                  "OnError must provide a single, non-template operator()");
    using on_error_trait = typename on_error_trait_helper::type;
    static_assert(std::is_same_v<typename on_error_trait::result_type, void>,
                  "OnError must return void");
    using on_error_args = typename on_error_trait::decayed_arg_types;
    static_assert(std::is_same_v<on_error_args, type_list<error>>,
                  "OnError must accept a single argument of type caf::error");
  }

  /// Points to the parent actor.
  scheduled_actor* self_;

  /// Stores the ID of the message we are waiting for.
  message_id mid_;

  /// Stores a handle to the in-flight timeout.
  disposable pending_timeout_;
};

/// Similar to `event_based_response_handle`, but also holds the `disposable`
/// for the delayed request message.
template <class Result>
class event_based_delayed_response_handle {
public:
  using decorated_type = event_based_response_handle<Result>;

  // -- constructors, destructors, and assignment operators --------------------

  event_based_delayed_response_handle(scheduled_actor* self, message_id mid,
                                      disposable pending_timeout,
                                      disposable pending_request)
    : decorated_(self, mid, std::move(pending_timeout)),
      pending_request_(std::move(pending_request)) {
    // nop
  }

  // -- then and await ---------------------------------------------------------

  /// @copydoc event_based_response_handle::await
  template <class OnValue, class OnError>
  disposable await(OnValue on_value, OnError on_error) && {
    std::move(decorated_).await(std::move(on_value), std::move(on_error));
    return std::move(pending_request_);
  }

  /// @copydoc event_based_response_handle::await
  template <class OnValue>
  disposable await(OnValue on_value) && {
    std::move(decorated_).await(std::move(on_value));
    return std::move(pending_request_);
  }

  /// @copydoc event_based_response_handle::then
  template <class OnValue, class OnError>
  disposable then(OnValue on_value, OnError on_error) && {
    std::move(decorated_).then(std::move(on_value), std::move(on_error));
    return std::move(pending_request_);
  }

  /// @copydoc event_based_response_handle::then
  template <class OnValue>
  disposable then(OnValue on_value) && {
    std::move(decorated_).then(std::move(on_value));
    return std::move(pending_request_);
  }

  // -- conversions ------------------------------------------------------------

  /// @copydoc event_based_response_handle::as_observable
  template <class T = Result, class Self = scheduled_actor>
  flow::assert_scheduled_actor_hdr_t<
    flow::observable<detail::event_based_response_handle_res_t<T>>>
  as_observable() && {
    return std::move(decorated_).template as_observable<T, Self>();
  }

  // -- properties -------------------------------------------------------------

  /// Returns the decorated handle.
  decorated_type& decorated() {
    return decorated_;
  }

  /// @copydoc decorated
  const decorated_type& decorated() const {
    return decorated_;
  }

  /// Returns the handle to the in-flight request message if the request was
  /// delayed/scheduled. Otherwise, returns an empty handle.
  disposable& pending_request() {
    return pending_request_;
  }

  /// @copydoc pending_request
  const disposable& pending_request() const {
    return pending_request_;
  }

private:
  /// The wrapped handle type.
  decorated_type decorated_;

  /// Stores a handle to the in-flight request if the request messages was
  /// delayed/scheduled.
  disposable pending_request_;
};

// tuple-like access for event_based_delayed_response_handle

template <size_t I, class Result>
decltype(auto) get(caf::event_based_delayed_response_handle<Result>& x) {
  if constexpr (I == 0) {
    return x.decorated();
  } else {
    static_assert(I == 1);
    return x.pending_request();
  }
}

template <size_t I, class Result>
decltype(auto) get(const caf::event_based_delayed_response_handle<Result>& x) {
  if constexpr (I == 0) {
    return x.decorated();
  } else {
    static_assert(I == 1);
    return x.pending_request();
  }
}

template <size_t I, class Result>
decltype(auto) get(caf::event_based_delayed_response_handle<Result>&& x) {
  if constexpr (I == 0) {
    return std::move(x.decorated());
  } else {
    static_assert(I == 1);
    return std::move(x.pending_request());
  }
}

} // namespace caf

// enable structured bindings for event_based_delayed_response_handle

namespace std {

template <class Result>
struct tuple_size<caf::event_based_delayed_response_handle<Result>> {
  static constexpr size_t value = 2;
};

template <class Result>
struct tuple_element<0, caf::event_based_delayed_response_handle<Result>> {
  using type = caf::event_based_response_handle<Result>;
};

template <class Result>
struct tuple_element<1, caf::event_based_delayed_response_handle<Result>> {
  using type = caf::disposable;
};

} // namespace std
