// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/none.hpp"

namespace caf {

/// Tag type to indicate that the sender of a message is a dynamically typed
/// actor.
struct dynamically_typed {
  using signatures = none_t;
};

} // namespace caf
