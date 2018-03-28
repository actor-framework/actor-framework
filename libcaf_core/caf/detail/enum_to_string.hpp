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

#include <type_traits>

namespace caf {
namespace detail {

/// Converts x to its underlying type and fetches the name from the
/// lookup table. Assumes consecutive enum values.
template <class E, size_t N>
const char* enum_to_string(E x, const char* (&lookup_table)[N]) {
  auto index = static_cast<typename std::underlying_type<E>::type>(x);
  return index < N ? lookup_table[index] : "<unknown>";
}

} // namespace detail
} // namespace caf

