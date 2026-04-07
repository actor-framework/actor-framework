// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/policy/event_based_requester.hpp"

namespace caf::policy {

/// Policy for configuring @ref mailer for typed event-based actors.
template <class MessagingInterface>
struct typed_event_based_requester {
  using untyped = event_based_requester;

  /// The type of the self pointer that will be passed to @ref mailer.
  using self_pointer = untyped::self_pointer;

  /// Messaging interface for the sending actor.
  using signatures = typename MessagingInterface::signatures;

  /// The guard type for setting and resetting thread-local context.
  using context_guard = untyped::context_guard;

  /// Whether to enable the `delegate` method.
  static constexpr bool enable_delegate = untyped::enable_delegate;

  /// Whether to enable the `request` method.
  static constexpr bool enable_request = untyped::enable_request;

  /// Whether to enable the `fan_out_request` method.
  static constexpr bool enable_fan_out_request
    = untyped::enable_fan_out_request;

  /// Maps a response type to a event-based response handle.
  template <class T>
  using response_handle = typename untyped::template response_handle<T>;

  /// Maps a response type to a event-based delayed response handle.
  template <class T>
  using delayed_response_handle =
    typename untyped::template delayed_response_handle<T>;

  /// Maps a response type to a event-based fan-out response handle.
  template <class Policy, class T>
  using fan_out_response_handle =
    typename untyped::template fan_out_response_handle<Policy, T>;

  /// Maps a response type to a event-based fan-out delayed response handle.
  template <class Policy, class T>
  using fan_out_delayed_response_handle =
    typename untyped::template fan_out_delayed_response_handle<Policy, T>;
};

} // namespace caf::policy
