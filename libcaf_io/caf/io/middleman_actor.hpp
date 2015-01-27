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

#ifndef CAF_IO_MIDDLEMAN_ACTOR_HPP
#define CAF_IO_MIDDLEMAN_ACTOR_HPP

#include "caf/fwd.hpp"
#include "caf/typed_actor.hpp"

namespace caf {
namespace io {

/**
 * A message passing interface for asynchronous networking operations.
 *
 * The interface implements the following pseudo code.
 * ~~~
 * interface middleman_actor {
 *   using put_result = either (ok_atom, uint16_t port)
 *                      or     (error_atom, std::string error_string);
 *
 *   put_result <- (put_atom, actor_addr whom, uint16_t port,
 *                  std::string addr, bool reuse_addr);
 *
 *   put_result <- (put_atom, actor_addr whom,
 *                  uint16_t port, std::string addr);
 *
 *   put_result <- (put_atom, actor_addr whom, uint16_t port, bool reuse_addr);
 *
 *   put_result <- (put_atom, actor_addr whom, uint16_t port);
 *
 *   using get_result = either (ok_atom, actor_addr remote_address)
 *                      or     (error_atom, std::string error_string);
 *
 *   get_result <- (get_atom, std::string hostname, uint16_t port);
 *
 *   get_result <- (get_atom, std::string hostname, uint16_t port,
 *                  std::set<std::string> expected_ifs);
 *
 *   using delete_result = either (ok_atom)
 *                         or     (error_atom, std::string error_string);
 *
 *   delete_result <- (delete_atom, actor_addr whom);
 *
 *   delete_result <- (delete_atom, actor_addr whom, uint16_t port);
 * }
 * ~~~
 *
 * The `middleman_actor` actor offers the following operations:
 * * `PUT` establishes a new `port <-> actor` mapping and  returns the actual
 *   port in use on success. When passing  0 as `port` parameter,
 *   this is the only way to learn which port was used.
 *   Parameter     | Description
 *   --------------|------------------------------------------------------------
 *   whom          | Actor that should be published at port.
 *   port          | Unused TCP port or 0 for any.
 *   addr          | Optional; the IP address to listen to or INADDR_ANY
 *   reuse_addr    | Optional; enable SO_REUSEPORT option (default: false)
 * * `GET` queries remote node and returns an `actor_addr` to the remote
 *   actor on success. This handle must be cast to either `actor` or
 *   `typed_actor` using `actor_cast`.
 *   Parameter     | Description
 *   --------------|------------------------------------------------------------
 *   hostname      | Valid hostname or IP address.
 *   port          | TCP port.
 *   expected_ifs  | Optional; Interface of typed remote actor.
 * * `DELETE` removes either all `port <-> actor` mappings for an actor or
 *   only a single one if the optional `port` parameter is set.
 *   Parameter     | Description
 *   --------------|------------------------------------------------------------
 *   whom          | Published actor.
 *   port          | Optional; remove only a single mapping.
 */
using middleman_actor =
  typed_actor<
    replies_to<put_atom, actor_addr, uint16_t, std::string, bool>
    ::with_either<ok_atom, uint16_t>
    ::or_else<error_atom, std::string>,
    replies_to<put_atom, actor_addr, uint16_t, std::string>
    ::with_either<ok_atom, uint16_t>
    ::or_else<error_atom, std::string>,
    replies_to<put_atom, actor_addr, uint16_t, bool>
    ::with_either<ok_atom, uint16_t>
    ::or_else<error_atom, std::string>,
    replies_to<put_atom, actor_addr, uint16_t>
    ::with_either<ok_atom, uint16_t>
    ::or_else<error_atom, std::string>,
    replies_to<get_atom, std::string, uint16_t>
    ::with_either<ok_atom, actor_addr>
    ::or_else<error_atom, std::string>,
    replies_to<get_atom, std::string, uint16_t, std::set<std::string>>
    ::with_either<ok_atom, actor_addr>
    ::or_else<error_atom, std::string>,
    replies_to<delete_atom, actor_addr>
    ::with_either<ok_atom>
    ::or_else<error_atom, std::string>,
    replies_to<delete_atom, actor_addr, uint16_t>
    ::with_either<ok_atom>
    ::or_else<error_atom, std::string>>;

/**
 * Returns a handle for asynchronous networking operations.
 */
middleman_actor get_middleman_actor(); // implemented in middleman.cpp

} // namespace io
} // namespace caf

#endif // CAF_IO_MIDDLEMAN_ACTOR_HPP

