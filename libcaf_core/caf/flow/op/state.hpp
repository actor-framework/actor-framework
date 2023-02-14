// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/default_enum_inspect.hpp"
#include "caf/detail/core_export.hpp"

namespace caf::flow::op {

/// Represents the state of a flow operator. Some operators only use a subset of
/// the possible states.
enum class state {
  idle = 0b0000'0001,
  running = 0b0000'0010,
  completed = 0b0000'0100,
  aborted = 0b0000'1000,
  disposed = 0b0001'0000,
};

/// Checks whether `x` is either `completed` or `aborted`.
constexpr bool has_shut_down(state x) {
  return (static_cast<int>(x) & 0b0000'1100) != 0;
}

/// @relates state
CAF_CORE_EXPORT std::string to_string(state);

/// @relates state
CAF_CORE_EXPORT bool from_string(std::string_view, state&);

/// @relates state
CAF_CORE_EXPORT bool from_integer(std::underlying_type_t<state>, state&);

/// @relates state
template <class Inspector>
bool inspect(Inspector& f, state& x) {
  return default_enum_inspect(f, x);
}

} // namespace caf::flow::op
