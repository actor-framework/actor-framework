// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/fwd.hpp"

#include <optional>

namespace caf::detail {

/// Implementation detail for `left`.
template <class Left, class Right>
struct left_oracle {
  using type = Left;
};

/// Evaluates to `Left`.
template <class Left, class Right>
using left = typename left_oracle<Left, Right>::type;

/// Trivial cast that can only be used if U is implicitly convertible to T.
template <class T, class U>
T implicit_cast(U ptr) noexcept {
  return ptr;
}

/// Implementation detail for `always_false`.
template <class...>
struct always_false_oracle {
  static constexpr bool value = false;
};

/// Evaluates to `false`. Useful for failing static assertions.
template <class... Ts>
inline constexpr bool always_false = always_false_oracle<Ts...>::value;

template <class T>
struct unboxed_trait {
  using type = T;
};

template <class T>
struct unboxed_trait<std::optional<T>> {
  using type = T;
};

template <class T>
struct unboxed_trait<expected<T>> {
  using type = T;
};

/// Evaluates to the value type of `std::optional` or `caf::expected`.
template <class T>
using unboxed = typename unboxed_trait<T>::type;

} // namespace caf::detail
