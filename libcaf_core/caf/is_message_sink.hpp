// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <type_traits>

#include "caf/fwd.hpp"

namespace caf {

template <class T>
struct is_message_sink : std::false_type {};

template <>
struct is_message_sink<actor> : std::true_type {};

template <>
struct is_message_sink<group> : std::true_type {};

template <class... Ts>
struct is_message_sink<typed_actor<Ts...>> : std::true_type {};

} // namespace caf
