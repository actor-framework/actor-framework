/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include <set>
#include <string>
#include <cstdint>

#include "caf/actor_system.hpp"

#include "caf/io/middleman.hpp"

namespace caf {
namespace io {

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

} // namespace io
} // namespace caf

