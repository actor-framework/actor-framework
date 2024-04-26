// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/allowed_unsafe_message_type.hpp"
#include "caf/detail/is_complete.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/fwd.hpp"
#include "caf/span.hpp"

#include <string>
#include <type_traits>
#include <vector>

namespace caf {

/// Wraps tag types for static dispatching.
struct inspector_access_type {
  /// Flags types that provide an `inspector_access` specialization.
  struct specialization {};

  /// Flags types that provide an `inspect()` overload.
  struct inspect {};

  /// Flags types with builtin support of the inspection API.
  struct builtin {};

  /// Flags types with builtin support via `Inspector::builtin_inspect`.
  struct builtin_inspect {};

  /// Flags stateless message types.
  struct empty {};

  /// Flags allowed unsafe message types.
  struct unsafe {};

  /// Flags types with `std::tuple`-like API.
  struct tuple {};

  /// Flags types with `std::map`-like API.
  struct map {};

  /// Flags types with `std::vector`-like API.
  struct list {};

  /// Flags types without any default access.
  struct none {};
};

/// @relates inspector_access_type
template <class Inspector, class T>
constexpr auto inspect_access_type() {
  using namespace detail;
  // Order: unsafe (error) > C Array > builtin > builtin_inspect
  //        > inspector_access > inspect > trivial > defaults.
  if constexpr (is_allowed_unsafe_message_type_v<T>)
    return inspector_access_type::unsafe{};
  else if constexpr (std::is_array<T>::value)
    return inspector_access_type::tuple{};
  else if constexpr (detail::is_builtin_inspector_type<
                       T, Inspector::is_loading>::value)
    return inspector_access_type::builtin{};
  else if constexpr (has_builtin_inspect_v<Inspector, T>)
    return inspector_access_type::builtin_inspect{};
  else if constexpr (detail::is_complete<inspector_access<T>>)
    return inspector_access_type::specialization{};
  else if constexpr (has_inspect_overload_v<Inspector, T>)
    return inspector_access_type::inspect{};
  else if constexpr (std::is_empty<T>::value)
    return inspector_access_type::empty{};
  else if constexpr (is_stl_tuple_type_v<T>)
    return inspector_access_type::tuple{};
  else if constexpr (is_map_like_v<T>)
    return inspector_access_type::map{};
  else if constexpr (is_list_like_v<T>)
    return inspector_access_type::list{};
  else
    return inspector_access_type::none{};
}

} // namespace caf
