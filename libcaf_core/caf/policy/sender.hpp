// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/current_actor.hpp"
#include "caf/fwd.hpp"

namespace caf::policy {

/// Policy for configuring @ref mailer for an actor type that is using
/// dynamically typed messaging and only supports `send`.
struct sender {
  /// The type of the self pointer that will be passed to @ref mailer.
  using self_pointer = abstract_actor*;

  /// Messaging interface for the sending actor.
  using signatures = none_t;

  /// The guard type for setting and resetting thread-local context.
  using context_guard = detail::current_actor_guard;

  /// Whether to enable the `delegate` method.
  static constexpr bool enable_delegate = false;

  /// Whether to enable the `request` method.
  static constexpr bool enable_request = false;

  /// Whether to enable the `fan_out_request` method.
  static constexpr bool enable_fan_out_request = false;
};

} // namespace caf::policy
