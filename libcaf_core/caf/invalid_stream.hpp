// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/type_traits.hpp"

namespace caf {

struct invalid_stream_t {};

constexpr invalid_stream_t invalid_stream = invalid_stream_t{};

} // namespace caf
