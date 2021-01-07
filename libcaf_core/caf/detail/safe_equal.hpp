// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cmath> // fabs
#include <limits>
#include <type_traits>

namespace caf::detail {

/// Compares two values by using `operator==` unless two floating
/// point numbers are compared. In the latter case, the function
/// performs an epsilon comparison.
template <class T, typename U>
typename std::enable_if<
  !std::is_floating_point<T>::value && !std::is_floating_point<U>::value
    && !(std::is_same<T, U>::value && std::is_empty<T>::value),
  bool>::type
safe_equal(const T& lhs, const U& rhs) {
  return lhs == rhs;
}

template <class T, typename U>
typename std::enable_if<std::is_same<T, U>::value && std::is_empty<T>::value,
                        bool>::type
safe_equal(const T&, const U&) {
  return true;
}

template <class T, typename U>
typename std::enable_if<std::is_floating_point<T>::value
                          || std::is_floating_point<U>::value,
                        bool>::type
safe_equal(const T& lhs, const U& rhs) {
  using res_type = decltype(lhs - rhs);
  return std::fabs(lhs - rhs) <= std::numeric_limits<res_type>::epsilon();
}

} // namespace caf::detail
