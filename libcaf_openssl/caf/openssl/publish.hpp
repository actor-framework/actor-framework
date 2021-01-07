// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstdint>
#include <set>
#include <string>

#include "caf/actor_cast.hpp"
#include "caf/actor_control_block.hpp"
#include "caf/detail/openssl_export.hpp"
#include "caf/error.hpp"
#include "caf/fwd.hpp"
#include "caf/sec.hpp"
#include "caf/typed_actor.hpp"

namespace caf::openssl {

/// @private
CAF_OPENSSL_EXPORT expected<uint16_t>
publish(actor_system& sys, const strong_actor_ptr& whom,
        std::set<std::string>&& sigs, uint16_t port, const char* cstr, bool ru);

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
  return publish(sys, actor_cast<strong_actor_ptr>(whom),
                 sys.message_types(whom), port, in, reuse);
}

} // namespace caf::openssl
