// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

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
