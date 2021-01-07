// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <type_traits>

namespace caf {

template <class T, int Pos>
struct sum_type_token {};

template <class T, int Pos>
constexpr std::integral_constant<int, Pos> pos(sum_type_token<T, Pos>) {
  return {};
}

} // namespace caf
