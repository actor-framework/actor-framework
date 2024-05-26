// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

namespace caf {

/// Tag type to indicate that an operation should take place only once.
struct once_t {};

/// Tag to indicate that an operation should take place only once.
inline constexpr once_t once = {};

} // namespace caf
