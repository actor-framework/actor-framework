// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include <chrono>
#include <cstdint>
#include <limits>

namespace caf {

/// A portable timespan type with nanosecond resolution.
using timespan = std::chrono::duration<int64_t, std::nano>;

/// Constant representing an infinite amount of time.
static constexpr timespan infinite
  = timespan{std::numeric_limits<int64_t>::max()};

/// Checks whether `value` represents an infinite amount of time.
constexpr bool is_infinite(timespan value) {
  return value == infinite;
}

} // namespace caf
