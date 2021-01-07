// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstdint>

#include "caf/actor_system.hpp"

#include "caf/io/middleman.hpp"

namespace caf::io {

/// Unpublishes `whom` by closing `port` or all assigned ports if `port == 0`.
/// @param whom Actor that should be unpublished at `port`.
/// @param port TCP port.
template <class Handle>
expected<void> unpublish(const Handle& whom, uint16_t port = 0) {
  if (!whom)
    return sec::invalid_argument;
  return whom.home_system().middleman().unpublish(whom, port);
}

} // namespace caf::io
