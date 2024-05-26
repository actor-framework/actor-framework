// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

namespace caf {

/// Empty struct tag for constructing a typed behavior from an untyped behavior.
struct unsafe_behavior_init_t {};

/// Convenience constant for constructing a typed behavior from an untyped
/// behavior.
constexpr unsafe_behavior_init_t unsafe_behavior_init
  = unsafe_behavior_init_t{};

} // namespace caf
