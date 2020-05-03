/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
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

namespace caf::detail {

template <class T, std::size_t = sizeof(T)>
std::true_type is_complete_impl(T*);

std::false_type is_complete_impl(...);

/// Checks whether T is a complete type. For example, passing a forward
/// declaration or undefined template specialization evaluates to `false`.
template <class T>
constexpr bool is_complete
  = decltype(is_complete_impl(std::declval<T*>()))::value;

} // namespace caf::detail
