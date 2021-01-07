// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/fwd.hpp"

namespace caf {

/**
 * Evaluates to true if `T` is a `typed_actor<...>`.
 */
template <class T>
struct is_typed_actor : std::false_type {};

template <class... Ts>
struct is_typed_actor<typed_actor<Ts...>> : std::true_type {};

} // namespace caf
