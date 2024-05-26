// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/io/middleman.hpp"

#include "caf/actor_system.hpp"

#include <cstdint>
#include <set>
#include <string>

namespace caf::io {

/// Tries to publish `whom` at `port` and returns either an `error` or the
/// bound port.
/// @param whom Actor that should be published at `port`.
/// @param port Unused TCP port.
/// @param in The IP address to listen to or `INADDR_ANY` if `in == nullptr`.
/// @param reuse Create socket using `SO_REUSEADDR`.
/// @returns The actual port the OS uses after `bind()`. If `port == 0`
///          the OS chooses a random high-level port.
template <class Handle>
expected<uint16_t> publish(const Handle& whom, uint16_t port,
                           const char* in = nullptr, bool reuse = false) {
  if (!whom)
    return sec::cannot_publish_invalid_actor;
  auto& sys = whom.home_system();
  return sys.middleman().publish(whom, port, in, reuse);
}

} // namespace caf::io
