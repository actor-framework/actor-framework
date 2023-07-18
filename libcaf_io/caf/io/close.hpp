// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/io/middleman.hpp"

#include "caf/actor_system.hpp"

#include <cstdint>

namespace caf::io {

/// Closes port `port` regardless of whether an actor is published to it.
inline expected<void> close(actor_system& sys, uint16_t port) {
  return sys.middleman().close(port);
}

} // namespace caf::io
