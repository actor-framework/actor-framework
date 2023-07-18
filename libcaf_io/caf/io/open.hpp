// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/io/middleman.hpp"

#include "caf/actor_system.hpp"

#include <cstdint>

namespace caf::io {

/// Tries to open a port for other CAF instances to connect to.
/// @experimental
inline expected<uint16_t> open(actor_system& sys, uint16_t port,
                               const char* in = nullptr, bool reuse = false) {
  return sys.middleman().open(port, in, reuse);
}

} // namespace caf::io
