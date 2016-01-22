/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_UTILITY_HPP
#define CAF_UTILITY_HPP

#include <ostream>
#include <type_traits>

namespace caf {

/// Inserts `x` into stream `os`.
/// Attempts `os << x` first and then `os << to_string(x)`.
/// ADL will pull in namespace `std` and that of `T`.
template <class CharT, class Traits, class T>
typename std::enable_if<std::is_same<CharT, char>::value,
                        std::basic_ostream<CharT, Traits>&>::type
operator<<(std::basic_ostream<CharT, Traits>& os, const T& x) {
  return os << to_string(x);
}

} // namespace caf

#endif // CAF_UTILITY_HPP
