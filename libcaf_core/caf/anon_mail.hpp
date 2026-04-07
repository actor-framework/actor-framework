// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/mailer.hpp"
#include "caf/none.hpp"
#include "caf/unit.hpp"

namespace caf::detail {

struct anon_sender {
  using self_pointer = unit_t;

  /// Messaging interface for the sending actor.
  using signatures = none_t;

  /// The guard type for setting and resetting thread-local context.
  using context_guard = unit_t;

  /// Whether to enable the `delegate` method.
  static constexpr bool enable_delegate = false;

  /// Whether to enable the `request` method.
  static constexpr bool enable_request = false;

  /// Whether to enable the `fan_out_request` method.
  static constexpr bool enable_fan_out_request = false;
};

} // namespace caf::detail

namespace caf {

/// Entry point for sending an anonymous message to an actor.
template <class... Args>
[[nodiscard]] auto anon_mail(Args&&... args) {
  using mailer_policy = detail::anon_sender;
  return make_mailer<mailer_policy>(unit, std::forward<Args>(args)...);
}

} // namespace caf
