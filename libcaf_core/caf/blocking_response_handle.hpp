// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/abstract_blocking_actor.hpp"
#include "caf/actor_traits.hpp"
#include "caf/blocking_actor.hpp"
#include "caf/catch_all.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/detail/typed_actor_util.hpp"
#include "caf/flow/fwd.hpp"
#include "caf/message_id.hpp"
#include "caf/none.hpp"
#include "caf/sec.hpp"
#include "caf/system_messages.hpp"
#include "caf/typed_behavior.hpp"

#include <type_traits>

namespace caf {

/// This helper class identifies an expected response message and enables
/// `request(...).then(...)`.
template <class Result>
class blocking_response_handle {
public:
  // -- constructors, destructors, and assignment operators --------------------

  blocking_response_handle(abstract_blocking_actor* self, message_id mid,
                           timespan timeout)
    : self_(self), mid_(mid), timeout_(timeout) {
    // nop
  }

  template <class OnValue, class OnError>
  void receive(OnValue on_value, OnError on_error) && {
    type_check<OnValue, OnError>();
    auto bhvr = behavior{std::move(on_value), std::move(on_error)};
    self_->do_receive(mid_, bhvr, timeout_);
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
    if constexpr (!std::is_same_v<Result, message>) {
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
  abstract_blocking_actor* self_;

  /// Stores the ID of the message we are waiting for.
  message_id mid_;

  /// Stores the timeout for the response.
  timespan timeout_;
};

/// Similar to `blocking_response_handle`, but also holds the `disposable`
/// for the delayed request message.
template <class Result>
class blocking_delayed_response_handle {
public:
  using decorated_type = blocking_response_handle<Result>;

  // -- constructors, destructors, and assignment operators --------------------

  blocking_delayed_response_handle(abstract_blocking_actor* self,
                                   message_id mid, timespan timeout,
                                   disposable pending_request)
    : decorated_(self, mid, timeout),
      pending_request_(std::move(pending_request)) {
    // nop
  }

  // -- then and await ---------------------------------------------------------

  template <class OnValue, class OnError>
  void receive(OnValue on_value, OnError on_error) && {
    std::move(decorated_).receive(std::move(on_value), std::move(on_error));
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

// tuple-like access for blocking_delayed_response_handle

template <size_t I, class Result>
decltype(auto) get(caf::blocking_delayed_response_handle<Result>& x) {
  if constexpr (I == 0) {
    return x.decorated();
  } else {
    static_assert(I == 1);
    return x.pending_request();
  }
}

template <size_t I, class Result>
decltype(auto) get(const caf::blocking_delayed_response_handle<Result>& x) {
  if constexpr (I == 0) {
    return x.decorated();
  } else {
    static_assert(I == 1);
    return x.pending_request();
  }
}

template <size_t I, class Result>
decltype(auto) get(caf::blocking_delayed_response_handle<Result>&& x) {
  if constexpr (I == 0) {
    return std::move(x.decorated());
  } else {
    static_assert(I == 1);
    return std::move(x.pending_request());
  }
}

} // namespace caf

// enable structured bindings for blocking_delayed_response_handle

namespace std {

template <class Result>
struct tuple_size<caf::blocking_delayed_response_handle<Result>> {
  static constexpr size_t value = 2;
};

template <class Result>
struct tuple_element<0, caf::blocking_delayed_response_handle<Result>> {
  using type = caf::blocking_response_handle<Result>;
};

template <class Result>
struct tuple_element<1, caf::blocking_delayed_response_handle<Result>> {
  using type = caf::disposable;
};

} // namespace std
