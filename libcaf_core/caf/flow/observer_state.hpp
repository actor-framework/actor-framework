// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/default_enum_inspect.hpp"
#include "caf/detail/core_export.hpp"

namespace caf::flow {

/// Represents the current state of an @ref observer.
enum class observer_state {
  /// Indicates that no callbacks were called yet.
  idle,
  /// Indicates that on_subscribe was called.
  subscribed,
  /// Indicates that on_complete was called.
  completed,
  /// Indicates that on_error was called.
  aborted
};

/// Returns whether `x` represents a final state, i.e., `completed`, `aborted`
/// or `disposed`.
constexpr bool is_final(observer_state x) noexcept {
  return static_cast<int>(x) >= static_cast<int>(observer_state::completed);
}

/// Returns whether `x` represents an active state, i.e., `idle` or
/// `subscribed`.
constexpr bool is_active(observer_state x) noexcept {
  return static_cast<int>(x) <= static_cast<int>(observer_state::subscribed);
}

/// @relates sec
CAF_CORE_EXPORT std::string to_string(observer_state);

/// @relates observer_state
CAF_CORE_EXPORT bool from_string(std::string_view, observer_state&);

/// @relates observer_state
CAF_CORE_EXPORT bool from_integer(std::underlying_type_t<observer_state>,
                                  observer_state&);

/// @relates observer_state
template <class Inspector>
bool inspect(Inspector& f, observer_state& x) {
  return default_enum_inspect(f, x);
}

} // namespace caf::flow
