// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/actor.hpp"
#include "caf/actor_traits.hpp"

#include <functional>
#include <type_traits>

namespace caf::detail {

template <class T>
struct spawn_fwd_convert : std::false_type {};

template <>
struct spawn_fwd_convert<scoped_actor> : std::true_type {};

template <class T>
struct spawn_fwd_convert<T*> {
  static constexpr bool value = actor_traits<T>::is_dynamically_typed;
};

/// Converts `scoped_actor` and pointers to actors to handles of type `actor`
/// but simply forwards any other argument in the same way `std::forward` does.
template <class T>
std::conditional_t<spawn_fwd_convert<std::remove_reference_t<T>>::value, actor,
                   T&&>
spawn_fwd(std::remove_reference_t<T>& arg) noexcept {
  return static_cast<T&&>(arg);
}

/// Converts `scoped_actor` and pointers to actors to handles of type `actor`
/// but simply forwards any other argument in the same way `std::forward` does.
template <class T>
std::conditional_t<spawn_fwd_convert<std::remove_reference_t<T>>::value, actor,
                   T&&>
spawn_fwd(std::remove_reference_t<T>&& arg) noexcept {
  static_assert(!std::is_lvalue_reference_v<T>,
                "silently converting an lvalue to an rvalue");
  return static_cast<T&&>(arg);
}

} // namespace caf::detail
