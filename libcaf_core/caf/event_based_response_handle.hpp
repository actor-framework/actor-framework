// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/abstract_scheduled_actor.hpp"
#include "caf/detail/metaprogramming.hpp"
#include "caf/detail/response_type_check.hpp"
#include "caf/disposable.hpp"
#include "caf/fwd.hpp"
#include "caf/message_id.hpp"
#include "caf/none.hpp"
#include "caf/type_list.hpp"

#include <utility>

namespace caf::detail {

template <class Result>
struct event_based_response_handle_oracle;

template <>
struct event_based_response_handle_oracle<message> {
  using type = event_based_response_handle<message>;
};

template <>
struct event_based_response_handle_oracle<type_list<void>> {
  using type = event_based_response_handle<>;
};

template <class... Results>
struct event_based_response_handle_oracle<type_list<Results...>> {
  using type = event_based_response_handle<Results...>;
};

template <class Result>
using event_based_response_handle_t =
  typename event_based_response_handle_oracle<Result>::type;

template <class Result>
struct event_based_delayed_response_handle_oracle;

template <>
struct event_based_delayed_response_handle_oracle<message> {
  using type = event_based_delayed_response_handle<message>;
};

template <>
struct event_based_delayed_response_handle_oracle<type_list<void>> {
  using type = event_based_delayed_response_handle<>;
};

template <class... Results>
struct event_based_delayed_response_handle_oracle<type_list<Results...>> {
  using type = event_based_delayed_response_handle<Results...>;
};

template <class Result>
using event_based_delayed_response_handle_t =
  typename event_based_delayed_response_handle_oracle<Result>::type;

} // namespace caf::detail

namespace caf {

/// Holds state for a event-based response handles.
struct event_based_response_handle_state {
  static constexpr bool is_fan_out = false;

  /// Points to the parent actor.
  abstract_scheduled_actor* self;

  /// Stores the ID of the message we are waiting for.
  message_id mid;

  /// Stores a handle to the in-flight timeout.
  disposable pending_timeout;
};

/// This helper class identifies an expected response message and enables
/// `request(...).then(...)`.
template <class... Results>
class event_based_response_handle {
public:
  // -- friends ----------------------------------------------------------------

  friend class scheduled_actor;

  // -- constants --------------------------------------------------------------

  static constexpr bool is_dynamically_typed
    = std::is_same_v<type_list<Results...>, type_list<message>>;

  static constexpr bool is_statically_typed = !is_dynamically_typed;

  // -- constructors, destructors, and assignment operators --------------------

  event_based_response_handle(abstract_scheduled_actor* self, message_id mid,
                              disposable pending_timeout)
    : state_{self, mid, std::move(pending_timeout)} {
    // nop
  }

  // -- then and await ---------------------------------------------------------

  template <class OnValue, class OnError>
  void await(OnValue on_value, OnError on_error) && {
    detail::response_type_check<OnValue, OnError, Results...>();
    auto bhvr = behavior{std::move(on_value), std::move(on_error)};
    state_.self->add_awaited_response_handler(
      state_.mid, std::move(bhvr), std::move(state_.pending_timeout));
  }

  template <class OnValue>
  void await(OnValue on_value) && {
    return std::move(*this).await(std::move(on_value),
                                  [self = state_.self](error& err) {
                                    self->call_error_handler(err);
                                  });
  }

  template <class OnValue, class OnError>
  void then(OnValue on_value, OnError on_error) && {
    detail::response_type_check<OnValue, OnError, Results...>();
    auto bhvr = behavior{std::move(on_value), std::move(on_error)};
    state_.self->add_multiplexed_response_handler(
      state_.mid, std::move(bhvr), std::move(state_.pending_timeout));
  }

  template <class OnValue>
  void then(OnValue on_value) && {
    return std::move(*this).then(std::move(on_value),
                                 [self = state_.self](error& err) {
                                   self->call_error_handler(err);
                                 });
  }

  template <class T = abstract_scheduled_actor>
    requires is_statically_typed
  auto as_single() && {
    // Note: templatized so we need the definition of actor_flow_context only
    //       when instantiating this function
    return detail::implicit_cast<T*>(state_.self)
      ->flow_context()
      .response_to_single(type_list_v<Results...>, state_);
  }

  template <class T, class... Ts>
    requires is_dynamically_typed
  auto as_single() && {
    // Note: templatized so we need the definition of actor_flow_context only
    //       when instantiating this function
    using self_type = detail::left<abstract_scheduled_actor, T>;
    return detail::implicit_cast<self_type*>(state_.self)
      ->flow_context()
      .response_to_single(type_list_v<T, Ts...>, state_);
  }

  template <class T = abstract_scheduled_actor>
    requires is_statically_typed
  auto as_observable() && {
    // Note: templatized so we need the definition of response_to_observable
    //       only when instantiating this function
    return detail::implicit_cast<T*>(state_.self)
      ->response_to_observable(type_list_v<Results...>, state_, none);
  }

  template <class T, class... Ts>
    requires is_dynamically_typed
  auto as_observable() && {
    // Note: templatized so we need the definition of response_to_observable
    //       only when instantiating this function
    using self_type = detail::left<abstract_scheduled_actor, T>;
    return detail::implicit_cast<self_type*>(state_.self)
      ->response_to_observable(type_list_v<T, Ts...>, state_, none);
  }

private:
  /// Holds the state for the handle.
  event_based_response_handle_state state_;
};

/// Similar to `event_based_response_handle`, but also holds the `disposable`
/// for the delayed request message.
template <class... Results>
class event_based_delayed_response_handle {
public:
  // -- member types -----------------------------------------------------------

  using decorated_type = event_based_response_handle<Results...>;

  // -- constants --------------------------------------------------------------

  static constexpr bool is_dynamically_typed
    = std::is_same_v<type_list<Results...>, type_list<message>>;

  static constexpr bool is_statically_typed = !is_dynamically_typed;

  // -- constructors, destructors, and assignment operators --------------------

  event_based_delayed_response_handle(abstract_scheduled_actor* self,
                                      message_id mid,
                                      disposable pending_timeout,
                                      disposable pending_request)
    : decorated(self, mid, std::move(pending_timeout)),
      pending_request(std::move(pending_request)) {
    // nop
  }

  // -- then and await ---------------------------------------------------------

  /// @copydoc event_based_response_handle::await
  template <class OnValue, class OnError>
  disposable await(OnValue on_value, OnError on_error) && {
    std::move(decorated).await(std::move(on_value), std::move(on_error));
    return std::move(pending_request);
  }

  /// @copydoc event_based_response_handle::await
  template <class OnValue>
  disposable await(OnValue on_value) && {
    std::move(decorated).await(std::move(on_value));
    return std::move(pending_request);
  }

  /// @copydoc event_based_response_handle::then
  template <class OnValue, class OnError>
  disposable then(OnValue on_value, OnError on_error) && {
    std::move(decorated).then(std::move(on_value), std::move(on_error));
    return std::move(pending_request);
  }

  /// @copydoc event_based_response_handle::then
  template <class OnValue>
  disposable then(OnValue on_value) && {
    std::move(decorated).then(std::move(on_value));
    return std::move(pending_request);
  }

  auto as_observable() && {
    return std::move(decorated).as_observable();
  }

  template <class T = abstract_scheduled_actor>
    requires is_statically_typed
  auto as_single() && {
    return std::move(decorated).template as_single<T>();
  }

  template <class T, class... Ts>
    requires is_dynamically_typed
  auto as_single() && {
    return std::move(decorated).template as_single<T, Ts...>();
  }

  template <class T = abstract_scheduled_actor>
    requires is_statically_typed
  auto as_observable() && {
    return std::move(decorated).template as_observable<T>();
  }

  template <class T, class... Ts>
    requires is_dynamically_typed
  auto as_observable() && {
    return std::move(decorated).template as_observable<T, Ts...>();
  }

  // -- properties -------------------------------------------------------------

  /// The wrapped handle type.
  decorated_type decorated;

  /// Stores a handle to the in-flight request if the request messages was
  /// delayed/scheduled.
  disposable pending_request;
};

// tuple-like access for event_based_delayed_response_handle

template <size_t I, class... Results>
decltype(auto) get(caf::event_based_delayed_response_handle<Results...>& x) {
  if constexpr (I == 0) {
    return x.decorated;
  } else {
    static_assert(I == 1);
    return x.pending_request;
  }
}

template <size_t I, class... Results>
decltype(auto)
get(const caf::event_based_delayed_response_handle<Results...>& x) {
  if constexpr (I == 0) {
    return x.decorated;
  } else {
    static_assert(I == 1);
    return x.pending_request;
  }
}

template <size_t I, class... Results>
decltype(auto) get(caf::event_based_delayed_response_handle<Results...>&& x) {
  if constexpr (I == 0) {
    return std::move(x.decorated);
  } else {
    static_assert(I == 1);
    return std::move(x.pending_request);
  }
}

} // namespace caf

// enable structured bindings for event_based_delayed_response_handle

namespace std {

template <class... Results>
struct tuple_size<caf::event_based_delayed_response_handle<Results...>> {
  static constexpr size_t value = 2;
};

template <class... Results>
struct tuple_element<0, caf::event_based_delayed_response_handle<Results...>> {
  using type = caf::event_based_response_handle<Results...>;
};

template <class... Results>
struct tuple_element<1, caf::event_based_delayed_response_handle<Results...>> {
  using type = caf::disposable;
};

} // namespace std
