/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include <cmath> // fabs
#include <limits>
#include <type_traits>

namespace caf {
namespace detail {

/// Compares two values by using `operator==` unless two floating
/// point numbers are compared. In the latter case, the function
/// performs an epsilon comparison.
template <class T, typename U>
typename std::enable_if<
  !std::is_floating_point<T>::value
  && !std::is_floating_point<U>::value
  && !(std::is_same<T, U>::value && std::is_empty<T>::value),
  bool
>::type
safe_equal(const T& lhs, const U& rhs) {
  return lhs == rhs;
}

template <class T, typename U>
typename std::enable_if<
  std::is_same<T, U>::value && std::is_empty<T>::value,
  bool
>::type
safe_equal(const T&, const U&) {
  return true;
}

template <class T, typename U>
typename std::enable_if<
  std::is_floating_point<T>::value || std::is_floating_point<U>::value,
  bool
>::type
safe_equal(const T& lhs, const U& rhs) {
  using res_type = decltype(lhs - rhs);
  return std::fabs(lhs - rhs) <= std::numeric_limits<res_type>::epsilon();
}

} // namespace detail
} // namespace caf

