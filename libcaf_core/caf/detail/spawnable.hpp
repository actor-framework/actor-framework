// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/infer_handle.hpp"

#include <type_traits>

namespace caf::detail {

/// Returns whether the function object `F` is spawnable from the actor
/// implementation `Impl` with arguments of type `Ts...`.
template <class F, class Impl, class... Ts>
constexpr bool spawnable() {
  return std::is_invocable_v<F, Ts...> || std::is_invocable_v<F, Impl*, Ts...>;
}

} // namespace caf::detail
