// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <type_traits>

#include "caf/fwd.hpp"

namespace caf {

/// Checks whether `T` is an `actor` or a `typed_actor<...>`.
template <class T>
struct is_actor_handle : std::false_type {};

template <>
struct is_actor_handle<actor> : std::true_type {};

template <class... Ts>
struct is_actor_handle<typed_actor<Ts...>> : std::true_type {};

} // namespace caf
