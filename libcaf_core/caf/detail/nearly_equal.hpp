// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace caf::detail {

/// Checks whether two floating point numbers are nearly equal.
inline bool nearly_equal(double lhs, double rhs) {
  auto diff = std::abs(lhs - rhs);
  return diff <= std::max(std::abs(lhs), std::abs(rhs)) * DBL_EPSILON;
}

} // namespace caf::detail
