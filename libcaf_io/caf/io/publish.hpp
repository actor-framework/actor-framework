/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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

#ifndef CAF_IO_PUBLISH_HPP
#define CAF_IO_PUBLISH_HPP

#include <cstdint>

#include "caf/actor.hpp"
#include "caf/actor_cast.hpp"
#include "caf/typed_actor.hpp"

namespace caf {
namespace io {

uint16_t publish_impl(abstract_actor_ptr whom, uint16_t port,
                      const char* in, bool reuse_addr);

/// Publishes `whom` at `port`. The connection is managed by the middleman.
/// @param whom Actor that should be published at `port`.
/// @param port Unused TCP port.
/// @param in The IP address to listen to or `INADDR_ANY` if `in == nullptr`.
/// @returns The actual port the OS uses after `bind()`. If `port == 0` the OS
///          chooses a random high-level port.
/// @throws bind_failure
inline uint16_t publish(caf::actor whom, uint16_t port,
                        const char* in = nullptr, bool reuse_addr = false) {
  if (! whom) {
    return 0;
  }
  return publish_impl(actor_cast<abstract_actor_ptr>(whom), port, in,
                      reuse_addr);
}

/// @copydoc publish(actor,uint16_t,const char*)
template <class... Sigs>
uint16_t typed_publish(typed_actor<Sigs...> whom, uint16_t port,
                       const char* in = nullptr, bool reuse_addr = false) {
  if (! whom) {
    return 0;
  }
  return publish_impl(actor_cast<abstract_actor_ptr>(whom), port, in,
                      reuse_addr);
}

} // namespace io
} // namespace caf

#endif // CAF_IO_PUBLISH_HPP
