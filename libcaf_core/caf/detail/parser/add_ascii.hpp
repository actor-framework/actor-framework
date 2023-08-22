
// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/config.hpp"
#include "caf/detail/parser/ascii_to_int.hpp"
#include "caf/detail/type_traits.hpp"

#include <limits>
#include <type_traits>

namespace caf::detail::parser {

// Sum up integers when parsing positive integers.
// @returns `false` on an overflow, otherwise `true`.
// @pre `isdigit(c) || (Base == 16 && isxdigit(c))`
// @warning can leave `x` in an intermediate state when retuning `false`
template <int Base, class T>
bool add_ascii(T& x, char c,
               std::enable_if_t<std::is_integral_v<T>, int> u = 0) {
  CAF_IGNORE_UNUSED(u);
  if (x > (std::numeric_limits<T>::max() / Base))
    return false;
  x *= static_cast<T>(Base);
  ascii_to_int<Base, T> f;
  auto y = f(c);
  if (x > (std::numeric_limits<T>::max() - y))
    return false;
  x += static_cast<T>(y);
  return true;
}

template <int Base, class T>
bool add_ascii(T& x, char c,
               std::enable_if_t<std::is_floating_point_v<T>, int> u = 0) {
  CAF_IGNORE_UNUSED(u);
  ascii_to_int<Base, T> f;
  x = (x * Base) + f(c);
  return true;
}

} // namespace caf::detail::parser
