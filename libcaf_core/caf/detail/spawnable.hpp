// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <type_traits>

#include "caf/detail/type_traits.hpp"
#include "caf/infer_handle.hpp"

namespace caf::detail {

/// Returns whether the function object `F` is spawnable from the actor
/// implementation `Impl` with arguments of type `Ts...`.
template <class F, class Impl, class... Ts>
constexpr bool spawnable() {
  return is_callable_with<F, Ts...>::value
         || is_callable_with<F, Impl*, Ts...>::value;
}

} // namespace caf::detail
