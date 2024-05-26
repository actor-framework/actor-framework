// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

namespace caf {

struct keep_behavior_t {
  constexpr keep_behavior_t() {
    // nop
  }
};

/// Policy tag that causes {@link event_based_actor::become} to
/// keep the current behavior available.
/// @relates local_actor
constexpr keep_behavior_t keep_behavior = keep_behavior_t{};

} // namespace caf
