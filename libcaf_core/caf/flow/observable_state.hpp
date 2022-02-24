// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/default_enum_inspect.hpp"
#include "caf/detail/core_export.hpp"

namespace caf::flow {

/// Represents the current state of an @ref observable.
enum class observable_state {
  /// Indicates that the observable is still waiting on some event or input.
  idle,
  /// Indicates that at least one observer subscribed.
  running,
  /// Indicates that the observable is waiting for observers to consume all
  /// produced items before shutting down.
  completing,
  /// Indicates that the observable properly shut down.
  completed,
  /// Indicates that the observable shut down due to an error.
  aborted,
  /// Indicates that dispose was called.
  disposed,
};

/// Returns whether `x` represents a final state, i.e., `completed`, `aborted`
/// or `disposed`.
constexpr bool is_final(observable_state x) noexcept {
  return static_cast<int>(x) >= static_cast<int>(observable_state::completed);
}

/// Returns whether `x` represents an active state, i.e., `idle` or `running`.
constexpr bool is_active(observable_state x) noexcept {
  return static_cast<int>(x) <= static_cast<int>(observable_state::running);
}

/// @relates observable_state
CAF_CORE_EXPORT std::string to_string(observable_state);

/// @relates observable_state
CAF_CORE_EXPORT bool from_string(string_view, observable_state&);

/// @relates observable_state
CAF_CORE_EXPORT bool from_integer(std::underlying_type_t<observable_state>,
                                  observable_state&);

/// @relates observable_state
template <class Inspector>
bool inspect(Inspector& f, observable_state& x) {
  return default_enum_inspect(f, x);
}

} // namespace caf::flow
