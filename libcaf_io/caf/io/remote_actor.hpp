/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENCE_ALTERNATIVE.       *
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

#include "caf/actor_cast.hpp"

#include "caf/io/middleman.hpp"

#include "caf/io/remote_actor_impl.hpp"
#include "caf/io/typed_remote_actor_helper.hpp"

namespace caf {
namespace io {

/**
 * Establish a new connection to the actor at `host` on given `port`.
 * @param host Valid hostname or IP address.
 * @param port TCP port.
 * @returns An {@link actor_ptr} to the proxy instance
 *          representing a remote actor.
 * @throws std::invalid_argument Thrown when connecting to a typed actor.
 */
inline actor remote_actor(const std::string& host, uint16_t port) {
  auto res = remote_actor_impl(std::set<std::string>{}, host, port);
  return actor_cast<actor>(res);
}

/**
 * Establish a new connection to the typed actor at `host` on given `port`.
 * @param host Valid hostname or IP address.
 * @param port TCP port.
 * @returns An {@link actor_ptr} to the proxy instance
 *          representing a typed remote actor.
 * @throws std::invalid_argument Thrown when connecting to a typed actor.
 */
template <class List>
typename typed_remote_actor_helper<List>::return_type
typed_remote_actor(const std::string& host, uint16_t port) {
  typed_remote_actor_helper<List> f;
  return f(host, port);
}

} // namespace io
} // namespace caf

#endif // CAF_IO_REMOTE_ACTOR_HPP
