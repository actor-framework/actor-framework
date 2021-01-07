// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/padded_size.hpp"

namespace caf::detail {

template <size_t Remaining, class T, class... Ts>
struct offset_at_helper {
  static constexpr size_t value = offset_at_helper<Remaining - 1, Ts...>::value
                                  + padded_size_v<T>;
};

template <class T, class... Ts>
struct offset_at_helper<0, T, Ts...> {
  static constexpr size_t value = 0;
};

template <size_t Index, class... Ts>
constexpr size_t offset_at = offset_at_helper<Index, Ts...>::value;

} // namespace caf::detail
