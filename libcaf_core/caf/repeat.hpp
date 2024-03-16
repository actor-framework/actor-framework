// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

namespace caf {

/// Tag type to indicate that an operation should be repeated.
struct repeat_t {};

/// Tag to indicate that an operation should be repeated.
inline constexpr repeat_t repeat = {};

} // namespace caf
