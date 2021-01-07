// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <type_traits>

#include "caf/detail/type_traits.hpp"

namespace caf::detail {

/// Moves the value from `x` if it is not a pointer (e.g., `optional` or
/// `expected`), returns `*x` otherwise.
template <class T>
T& move_if_not_ptr(T* x) {
  return *x;
}

/// Moves the value from `x` if it is not a pointer (e.g., `optional` or
/// `expected`), returns `*x` otherwise.
template <class T, class E = enable_if_t<!std::is_pointer<T>::value>>
auto move_if_not_ptr(T& x) -> decltype(std::move(*x)) {
  return std::move(*x);
}

} // namespace caf::detail
