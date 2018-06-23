
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

#include "caf/detail/parser/ascii_to_int.hpp"

namespace caf {
namespace detail {
namespace parser {

// Sum up integers when parsing positive integers.
// @returns `false` on an overflow, otherwise `true`.
// @pre `isdigit(c) || (Base == 16 && isxdigit(c))`
template <int Base, class T>
bool add_ascii(T& x, char c) {
  ascii_to_int<Base, T> f;
  auto before = x;
  x = (x * Base) + f(c);
  return before <= x;
}

} // namespace parser
} // namespace detail
} // namespace caf

