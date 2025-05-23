// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include <type_traits>
#include <utility>

namespace caf::detail {

// Backport of C++23's forward_like. Remove when upgrading to C+23.
// Source: https://en.cppreference.com/w/cpp/utility/forward_like
template <class T, class U>
constexpr auto&& forward_like(U&& x) noexcept {
  constexpr bool is_adding_const = std::is_const_v<std::remove_reference_t<T>>;
  if constexpr (std::is_lvalue_reference_v<T&&>) {
    if constexpr (is_adding_const)
      return std::as_const(x);
    else
      return static_cast<U&>(x);
  } else {
    if constexpr (is_adding_const)
      return std::move(std::as_const(x));
    else
      return std::move(x);
  }
}

} // namespace caf::detail
