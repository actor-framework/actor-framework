// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

namespace caf::async {

/// Policy type for having `consume` call `on_error` immediately after the
/// producer has aborted even if the buffer still contains items.
struct prioritize_errors_t {
  static constexpr bool calls_on_error = true;
};

/// @relates prioritize_errors_t
constexpr auto prioritize_errors = prioritize_errors_t{};

/// Policy type for having `consume` call `on_error` only after processing all
/// items from the buffer.
struct delay_errors_t {
  static constexpr bool calls_on_error = true;
};

/// @relates delay_errors_t
constexpr auto delay_errors = delay_errors_t{};

} // namespace caf::async
