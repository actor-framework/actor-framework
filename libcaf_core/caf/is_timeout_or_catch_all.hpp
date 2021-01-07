// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/catch_all.hpp"
#include "caf/timeout_definition.hpp"

namespace caf {

template <class T>
struct is_timeout_or_catch_all : std::false_type {};

template <class T>
struct is_timeout_or_catch_all<catch_all<T>> : std::true_type {};

template <class T>
struct is_timeout_or_catch_all<timeout_definition<T>> : std::true_type {};

} // namespace caf
