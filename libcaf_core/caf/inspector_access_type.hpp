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

#include "caf/detail/type_traits.hpp"

namespace caf {

/// Wraps tag types for static dispatching.
struct inspector_access_type {
  /// Flags stateless message types.
  struct empty {};
  /// Flags allowed unsafe message types.
  struct unsafe {};
  /// Flags builtin integral types.
  struct integral {};
  /// Flags type with user-defined `inspect` overloads.
  struct inspect {};
  /// Flags native C array types.
  struct array {};
  /// Flags types with `std::tuple`-like API.
  struct tuple {};
  /// Flags types with `std::map`-like API.
  struct map {};
  /// Flags types with `std::vector`-like API.
  struct list {};
};

/// @relates inspector_access_type
template <class T>
constexpr auto guess_inspector_access_type() {
  if constexpr (std::is_empty<T>::value) {
    return inspector_access_type::empty{};
  } else if constexpr (is_allowed_unsafe_message_type_v<T>) {
    return inspector_access_type::unsafe{};
  } else if constexpr (std::is_integral<T>::value) {
    return inspector_access_type::integral{};
  } else if constexpr (std::is_array<T>::value) {
    return inspector_access_type::array{};
  } else if constexpr (detail::is_stl_tuple_type_v<T>) {
    return inspector_access_type::tuple{};
  } else if constexpr (detail::is_map_like_v<T>) {
    return inspector_access_type::map{};
  } else {
    static_assert(detail::is_list_like_v<T>);
    return inspector_access_type::list{};
  }
}

} // namespace caf
