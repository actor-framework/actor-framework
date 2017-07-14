/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_OPENSSL_PUBLISH_HPP
#define CAF_OPENSSL_PUBLISH_HPP

#include <set>
#include <string>
#include <cstdint>

#include "caf/fwd.hpp"
#include "caf/sec.hpp"
#include "caf/error.hpp"
#include "caf/actor_cast.hpp"
#include "caf/typed_actor.hpp"
#include "caf/actor_control_block.hpp"

namespace caf {
namespace openssl {

/// @private
expected<uint16_t> publish(actor_system& sys, const strong_actor_ptr& whom,
                           std::set<std::string>&& sigs, uint16_t port,
                           const char* cstr, bool ru);

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

} // namespace openssl
} // namespace caf

#endif // CAF_OPENSSL_PUBLISH_HPP
