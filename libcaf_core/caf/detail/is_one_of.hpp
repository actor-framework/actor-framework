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

/// Checks wheter `T` is in the template parameter pack `Ts`.
template <class T, class... Ts>
struct is_one_of;

template <class T>
struct is_one_of<T> : std::false_type {};

template <class T, class... Ts>
struct is_one_of<T, T, Ts...> : std::true_type {};

template <class T, class U, class... Ts>
struct is_one_of<T, U, Ts...> : is_one_of<T, Ts...> {};

} // namespace detail
} // namespace caf

