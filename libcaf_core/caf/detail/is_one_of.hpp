// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include <type_traits>

namespace caf::detail {

/// Checks whether `T` is in the template parameter pack `Ts`.
template <class T, class... Ts>
struct is_one_of;

template <class T>
struct is_one_of<T> : std::false_type {};

template <class T, class... Ts>
struct is_one_of<T, T, Ts...> : std::true_type {};

template <class T, class U, class... Ts>
struct is_one_of<T, U, Ts...> : is_one_of<T, Ts...> {};

template <class T, class... Ts>
constexpr bool is_one_of_v = (std::is_same_v<T, Ts> || ...);

} // namespace caf::detail
