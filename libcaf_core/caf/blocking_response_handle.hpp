// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/abstract_blocking_actor.hpp"
#include "caf/actor_traits.hpp"
#include "caf/blocking_actor.hpp"
#include "caf/catch_all.hpp"
#include "caf/detail/response_type_check.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/detail/typed_actor_util.hpp"
#include "caf/flow/fwd.hpp"
#include "caf/message_id.hpp"
#include "caf/none.hpp"
#include "caf/sec.hpp"
#include "caf/system_messages.hpp"
#include "caf/typed_behavior.hpp"

#include <type_traits>

namespace caf::detail {

template <class Result>
struct blocking_response_handle_oracle;

template <>
struct blocking_response_handle_oracle<message> {
  using type = blocking_response_handle<message>;
};

template <>
struct blocking_response_handle_oracle<type_list<void>> {
  using type = blocking_response_handle<>;
};

template <class... Results>
struct blocking_response_handle_oracle<type_list<Results...>> {
  using type = blocking_response_handle<Results...>;
};

template <class Result>
using blocking_response_handle_t =
  typename blocking_response_handle_oracle<Result>::type;

template <class Result>
struct blocking_delayed_response_handle_oracle;

template <>
struct blocking_delayed_response_handle_oracle<message> {
  using type = blocking_delayed_response_handle<message>;
};

template <>
struct blocking_delayed_response_handle_oracle<type_list<void>> {
  using type = blocking_delayed_response_handle<>;
};

template <class... Results>
struct blocking_delayed_response_handle_oracle<type_list<Results...>> {
  using type = blocking_delayed_response_handle<Results...>;
};

template <class Result>
using blocking_delayed_response_handle_t =
  typename blocking_delayed_response_handle_oracle<Result>::type;

template <class... Ts>
struct expected_builder;

template <>
struct expected_builder<> {
  expected<void> result;
  void set_value() {
    // nop
  }
  void set_error(error x) {
    result = std::move(x);
  }
};

template <class T>
struct expected_builder<T> {
  expected<T> result;
  expected_builder() : result(T{}) {
    // nop
  }
  void set_value(T value) {
    result.emplace(std::move(value));
  }
  void set_error(error x) {
    result = std::move(x);
  }
};

template <class T1, class T2, class... Ts>
struct expected_builder<T1, T2, Ts...> {
  expected<std::tuple<T1, T2, Ts...>> result;
  expected_builder() : result(std::tuple{T1{}, T2{}, Ts{}...}) {
    // nop
  }
  void set_value(T1 arg1, T2 arg2, Ts... args) {
    result = std::tuple{std::move(arg1), std::move(arg2), std::move(args)...};
  }
  void set_error(error x) {
    result = std::move(x);
  }
};

} // namespace caf::detail

namespace caf {

/// Holds state for a blocking response handles.
struct blocking_response_handle_state {
  /// Points to the parent actor.
  abstract_blocking_actor* self;

  /// Stores the ID of the message we are waiting for.
  message_id mid;

  /// Stores the timeout for the response.
  timespan timeout;
};

/// This helper class identifies an expected response message and enables
/// `request(...).then(...)`.
template <class... Results>
class blocking_response_handle {
public:
  // -- constructors, destructors, and assignment operators --------------------

  blocking_response_handle(abstract_blocking_actor* self, message_id mid,
                           timespan timeout)
    : state_{self, mid, timeout} {
    // nop
  }

  template <class OnValue, class OnError>
  void receive(OnValue on_value, OnError on_error) && {
    detail::response_type_check<OnValue, OnError, Results...>();
    auto bhvr = behavior{std::move(on_value), std::move(on_error)};
    state_.self->do_receive(state_.mid, bhvr, state_.timeout);
  }

  auto receive() && {
    detail::expected_builder<Results...> bld;
    std::move(*this).receive(
      [&bld](Results... args) { bld.set_value(std::move(args)...); },
      [&bld](error& err) { bld.set_error(std::move(err)); });
    return std::move(bld.result);
  }

private:
  /// Holds the state for the handle.
  blocking_response_handle_state state_;
};

/// This helper class identifies an expected response message and enables
/// `request(...).then(...)`.
template <>
class blocking_response_handle<message> {
public:
  // -- constructors, destructors, and assignment operators --------------------

  blocking_response_handle(abstract_blocking_actor* self, message_id mid,
                           timespan timeout)
    : state_{self, mid, timeout} {
    // nop
  }

  template <class OnValue, class OnError>
  void receive(OnValue on_value, OnError on_error) && {
    detail::response_type_check<OnValue, OnError, message>();
    auto bhvr = behavior{std::move(on_value), std::move(on_error)};
    state_.self->do_receive(state_.mid, bhvr, state_.timeout);
  }

  template <class... Ts>
  auto receive() && {
    detail::expected_builder<Ts...> bld;
    std::move(*this).receive(
      [&bld](Ts... args) { bld.set_value(std::move(args)...); },
      [&bld](error& err) { bld.set_error(std::move(err)); });
    return std::move(bld.result);
  }

private:
  /// Holds the state for the handle.
  blocking_response_handle_state state_;
};

/// Similar to `blocking_response_handle`, but also holds the `disposable`
/// for the delayed request message.
template <class... Results>
class blocking_delayed_response_handle {
public:
  using decorated_type = blocking_response_handle<Results...>;

  // -- constructors, destructors, and assignment operators --------------------

  blocking_delayed_response_handle(abstract_blocking_actor* self,
                                   message_id mid, timespan timeout,
                                   disposable pending_request)
    : decorated_(self, mid, timeout),
      pending_request_(std::move(pending_request)) {
    // nop
  }

  // -- receive ----------------------------------------------------------------

  template <class OnValue, class OnError>
  void receive(OnValue on_value, OnError on_error) && {
    std::move(decorated_).receive(std::move(on_value), std::move(on_error));
  }

  auto receive() && {
    return std::move(decorated_).receive();
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

// Specialization for dynamically typed messages.
template <>
class blocking_delayed_response_handle<message> {
public:
  using decorated_type = blocking_response_handle<message>;

  // -- constructors, destructors, and assignment operators --------------------

  blocking_delayed_response_handle(abstract_blocking_actor* self,
                                   message_id mid, timespan timeout,
                                   disposable pending_request)
    : decorated_(self, mid, timeout),
      pending_request_(std::move(pending_request)) {
    // nop
  }

  // -- receive ----------------------------------------------------------------

  template <class OnValue, class OnError>
  void receive(OnValue on_value, OnError on_error) && {
    std::move(decorated_).receive(std::move(on_value), std::move(on_error));
  }

  template <class... Ts>
  auto receive() && {
    return std::move(decorated_).template receive<Ts...>();
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
