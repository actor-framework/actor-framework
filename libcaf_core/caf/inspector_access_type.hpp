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
#include "caf/fwd.hpp"

namespace caf {

/// Wraps tag types for static dispatching.
struct inspector_access_type {
  /// Flags types that provide an `inspect()` overload.
  struct inspect {};

  /// Flags builtin integral types.
  struct integral {};

  /// Flags types with builtin support via `.value()`.
  struct builtin {};

  /// Flags `enum` types.
  struct enumeration {};

  /// Flags stateless message types.
  struct empty {};

  /// Flags allowed unsafe message types.
  struct unsafe {};

  /// Flags native C array types.
  struct array {};

  /// Flags types with `std::tuple`-like API.
  struct tuple {};

  /// Flags type that specialize `inspector_access_boxed_value_traits`.
  struct boxed_value {};

  /// Flags types with `std::map`-like API.
  struct map {};

  /// Flags types with `std::vector`-like API.
  struct list {};

  /// Flags types without any default access.
  struct none {};
};

/// @relates inspector_access_type
template <class Inspector, class T>
constexpr auto guess_inspector_access_type() {
  // User-defined `inspect` overloads always come first.
  using namespace detail;
  if constexpr (is_inspectable<Inspector, T>::value) {
    return inspector_access_type::inspect{};
  } else if constexpr (std::is_integral<T>::value
                       && !std::is_same<T, bool>::value) {
    return inspector_access_type::integral{};
  } else if constexpr (std::is_array<T>::value) {
    return inspector_access_type::array{};
  } else if constexpr (is_trivially_inspectable<Inspector, T>::value) {
    return inspector_access_type::builtin{};
  } else if constexpr (std::is_enum<T>::value) {
    return inspector_access_type::enumeration{};
  } else if constexpr (std::is_empty<T>::value) {
    return inspector_access_type::empty{};
  } else if constexpr (is_allowed_unsafe_message_type_v<T>) {
    return inspector_access_type::unsafe{};
  } else if constexpr (is_stl_tuple_type_v<T>) {
    return inspector_access_type::tuple{};
  } else if constexpr (is_map_like_v<T>) {
    return inspector_access_type::map{};
  } else if constexpr (is_list_like_v<T>) {
    return inspector_access_type::list{};
  } else {
    return inspector_access_type::none{};
  }
}

} // namespace caf
