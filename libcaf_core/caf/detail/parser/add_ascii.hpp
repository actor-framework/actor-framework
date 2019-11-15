
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

#include <limits>

#include "caf/detail/parser/ascii_to_int.hpp"
#include "caf/detail/type_traits.hpp"

namespace caf::detail::parser {

// Sum up integers when parsing positive integers.
// @returns `false` on an overflow, otherwise `true`.
// @pre `isdigit(c) || (Base == 16 && isxdigit(c))`
// @warning can leave `x` in an intermediate state when retuning `false`
template <int Base, class T>
bool add_ascii(T& x, char c, enable_if_tt<std::is_integral<T>, int> u = 0) {
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
               enable_if_tt<std::is_floating_point<T>, int> u = 0) {
  CAF_IGNORE_UNUSED(u);
  ascii_to_int<Base, T> f;
  x = (x * Base) + f(c);
  return true;
}

} // namespace caf
