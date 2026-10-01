// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/fwd.hpp"

namespace caf::detail {

template <class T>
struct is_typed_actor_oracle {
  static constexpr bool value = false;
};

template <class... Ts>
struct is_typed_actor_oracle<typed_actor<Ts...>> {
  static constexpr bool value = true;
};

/// Evaluates to true if `T` is a `typed_actor<...>`.
template <class T>
inline constexpr bool is_typed_actor = is_typed_actor_oracle<T>::value;

template <class T>
struct is_actor_oracle {
  static constexpr bool value = false;
};

template <>
struct is_actor_oracle<actor> {
  static constexpr bool value = true;
};

/// Evaluates to true if `T` is a `actor`.
template <class T>
inline constexpr bool is_actor = is_actor_oracle<T>::value;

} // namespace caf::detail
