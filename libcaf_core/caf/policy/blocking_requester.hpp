// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/current_actor.hpp"
#include "caf/fwd.hpp"

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

} // namespace caf::detail

namespace caf::policy {

/// Policy for configuring @ref mailer for blocking actors.
struct blocking_requester {
  /// The type of the self pointer that will be passed to @ref mailer.
  using self_pointer = abstract_blocking_actor*;

  /// Messaging interface for the sending actor.
  using signatures = none_t;

  /// The guard type for setting and resetting thread-local context.
  using context_guard = detail::current_actor_guard;

  /// Whether to enable the `delegate` method.
  static constexpr bool enable_delegate = true;

  /// Whether to enable the `request` method.
  static constexpr bool enable_request = true;

  /// Whether to enable the `fan_out_request` method.
  static constexpr bool enable_fan_out_request = false;

  /// Maps a response type to a blocking response handle.
  template <class T>
  using response_handle = detail::blocking_response_handle_t<T>;

  /// Maps a response type to a blocking delayed response handle.
  template <class T>
  using delayed_response_handle = detail::blocking_delayed_response_handle_t<T>;
};

} // namespace caf::policy
