// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/caf_deprecated.hpp"
#include "caf/fwd.hpp"

namespace caf {

template <class T>
struct is_typed_actor : std::false_type {};

template <class... Ts>
struct is_typed_actor<typed_actor<Ts...>> : std::true_type {};

template <class T>
CAF_DEPRECATED("obsolete utility")
inline constexpr bool is_typed_actor_v = is_typed_actor<T>::value;

} // namespace caf
