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

#include "caf/actor_cast.hpp"

#include "caf/io/middleman.hpp"

namespace caf {
namespace io {

abstract_actor_ptr remote_actor_impl(const std::set<std::string>& ifs,
                                     const std::string& host, uint16_t port);

template <class List>
struct typed_remote_actor_helper;

template <template <class...> class List, class... Ts>
struct typed_remote_actor_helper<List<Ts...>> {
  using return_type = typed_actor<Ts...>;
  template <class... Vs>
  return_type operator()(Vs&&... vs) {
    auto iface = return_type::message_types();
    auto tmp = remote_actor_impl(std::move(iface), std::forward<Vs>(vs)...);
    return actor_cast<return_type>(tmp);
  }
};

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
