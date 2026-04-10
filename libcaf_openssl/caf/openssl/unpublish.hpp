// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/openssl/manager.hpp"

#include "caf/actor_control_block.hpp"
#include "caf/error.hpp"
#include "caf/expected.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/sec.hpp"

#include <cstdint>

namespace caf::openssl {

/// Unpublishes `whom` by closing `port` or all assigned ports if `port == 0`.
/// @param whom Actor that should be unpublished at `port`.
/// @param port TCP port.
template <class Handle>
expected<void> unpublish(const Handle& whom, uint16_t port = 0) {
  if (!whom)
    return sec::invalid_argument;
  auto& sys = whom.home_system();
  auto self = scoped_actor{sys};
  return self->mail(unpublish_atom_v, whom, port)
    .request(sys.openssl_manager().actor_handle(), infinite)
    .receive();
}

} // namespace caf::openssl
