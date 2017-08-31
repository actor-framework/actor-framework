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

#ifndef CAF_IO_REMOTE_ACTOR_HPP
#define CAF_IO_REMOTE_ACTOR_HPP

#include <set>
#include <string>
#include <cstdint>

#include "caf/actor_system.hpp"

#include "caf/io/middleman.hpp"

namespace caf {
namespace io {

/// Establish a new connection to the actor at `host` on given `port`.
/// @param host Valid hostname or IP address.
/// @param port TCP port.
/// @returns An `actor` to the proxy instance representing
///          a remote actor or an `error`.
template <class ActorHandle = actor>
expected<ActorHandle> remote_actor(actor_system& sys, std::string host,
                                   uint16_t port) {
  detail::type_list<ActorHandle> tk;
  return sys.middleman().remote_actor(sys, sys.message_types(tk),
                                      std::move(host), port);
}

} // namespace io
} // namespace caf

#endif // CAF_IO_REMOTE_ACTOR_HPP
