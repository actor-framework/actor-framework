// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstdint>
#include <set>
#include <string>

#include "caf/actor_system.hpp"

#include "caf/io/middleman.hpp"

namespace caf::io {

/// Establish a new connection to the actor at `host` on given `port`.
/// @param host Valid hostname or IP address.
/// @param port TCP port.
/// @returns An `actor` to the proxy instance representing
///          a remote actor or an `error`.
template <class ActorHandle = actor>
expected<ActorHandle>
remote_actor(actor_system& sys, std::string host, uint16_t port) {
  return sys.middleman().remote_actor<ActorHandle>(std::move(host), port);
}

} // namespace caf::io
