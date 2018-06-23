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

namespace caf {
namespace detail {
namespace parser {

/// Returns whether `c` is a valid digit for a given base.
template <int Base>
bool is_digit(char c);

template <>
inline bool is_digit<2>(char c) {
  return c == '0' || c == '1';
}

template <>
inline bool is_digit<8>(char c) {
  return c >= '0' && c <= '7';
}

template <>
inline bool is_digit<10>(char c) {
  return c >= '0' && c <= '9';
}

template <>
inline bool is_digit<16>(char c) {
  return (c >= '0' && c <= '9')
         || (c >= 'A' && c <= 'F')
         || (c >= 'a' && c <= 'f');
}

} // namespace parser
} // namespace detail
} // namespace caf

