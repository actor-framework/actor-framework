// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

namespace caf {

/// A list of types.
template <class... Ts>
struct type_list {
  constexpr type_list() {
    // nop
  }
};

template <class... Ts>
constexpr auto type_list_v = type_list<Ts...>{};

} // namespace caf
