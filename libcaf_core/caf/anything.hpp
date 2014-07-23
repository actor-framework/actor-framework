/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENCE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_ANYTHING_HPP
#define CAF_ANYTHING_HPP

#include <type_traits>

namespace caf {

/**
 * @brief Acts as wildcard expression in patterns.
 */
struct anything {};

/**
 * @brief Compares two instances of {@link anything}.
 * @returns @p false
 * @relates anything
 */
inline bool operator==(const anything&, const anything&) { return true; }

/**
 * @brief Compares two instances of {@link anything}.
 * @returns @p false
 * @relates anything
 */
inline bool operator!=(const anything&, const anything&) { return false; }

/**
 * @brief Checks wheter @p T is {@link anything}.
 */
template <class T>
struct is_anything {
  static constexpr bool value = std::is_same<T, anything>::value;

};

} // namespace caf

#endif // CAF_ANYTHING_HPP
