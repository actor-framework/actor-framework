// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstdint>
#include <set>
#include <string>

#include "caf/actor_control_block.hpp"
#include "caf/actor_system.hpp"
#include "caf/detail/openssl_export.hpp"
#include "caf/fwd.hpp"

namespace caf::openssl {

/// @private
CAF_OPENSSL_EXPORT expected<strong_actor_ptr>
remote_actor(actor_system& sys, const std::set<std::string>& mpi,
             std::string host, uint16_t port);

/// Establish a new connection to the actor at `host` on given `port`.
/// @param host Valid hostname or IP address.
/// @param port TCP port.
/// @returns An `actor` to the proxy instance representing
///          a remote actor or an `error`.
template <class ActorHandle = actor>
expected<ActorHandle>
remote_actor(actor_system& sys, std::string host, uint16_t port) {
  detail::type_list<ActorHandle> tk;
  auto res = remote_actor(sys, sys.message_types(tk), std::move(host), port);
  if (res)
    return actor_cast<ActorHandle>(std::move(*res));
  return std::move(res.error());
}

} // namespace caf::openssl
