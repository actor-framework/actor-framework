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

#ifndef CAF_IO_MIDDLEMAN_ACTOR_HPP
#define CAF_IO_MIDDLEMAN_ACTOR_HPP

#include "caf/fwd.hpp"
#include "caf/typed_actor.hpp"

namespace caf {
namespace io {

/// A message passing interface for asynchronous networking operations.
///
/// The interface implements the following pseudo code.
/// ~~~
/// using put_result =
///   either (ok_atom, uint16_t port)
///   or     (error_atom, string error_string);
///
/// using get_result =
///   either (ok_atom, actor_addr remote_address)
///   or     (error_atom, string error_string);
///
/// using delete_result =
///   either (ok_atom)
///   or     (error_atom, string error_string);
///
/// interface middleman_actor {
///   (put_atom, actor_addr whom, uint16_t port, string addr, bool reuse_addr)
///   -> put_result;
///
///   (put_atom, actor_addr whom, uint16_t port, string addr)
///   -> put_result;
///
///   (put_atom, actor_addr whom, uint16_t port, bool reuse_addr)
///   -> put_result;
///
///   (put_atom, actor_addr whom, uint16_t port)
///   -> put_result;
///
///   (get_atom, string hostname, uint16_t port)
///   -> get_result;
///
///   (get_atom, string hostname, uint16_t port, set<string> expected_ifs)
///   -> get_result;
///
///   (delete_atom, actor_addr whom)
///   -> delete_result;
///
///   (delete_atom, actor_addr whom, uint16_t port)
///   -> delete_result;
/// }
/// ~~~
///
/// The `middleman_actor` actor offers the following operations:
/// - `PUT` establishes a new `port <-> actor`
///   mapping and  returns the actual port in use on success.
///   Passing 0 as port instructs the OS to choose the next high-level port
///   available for binding.
///   Type       | Name       | Parameter Description
///   -----------|------------|--------------------------------------------------
///   put_atom   |            | Identifies `PUT` operations.
///   actor_addr | whom       | Actor that should be published at given port.
///   uint16_t   | port       | Unused TCP port or 0 for any.
///   string     | addr       | Optional; IP address to listen to or `INADDR_ANY`
///   bool       | reuse_addr | Optional; enable SO_REUSEPORT option
///
/// - `GET` queries a remote node and returns an `actor_addr` to the remote actor
///   on success. This handle must be cast to either `actor` or `typed_actor`
///   using `actor_cast`.
///   Type        | Name        | Parameter Description
///   ------------|-------------|------------------------------------------------
///   get_atom    |             | Identifies `GET` operations.
///   string      | hostname    | Valid hostname or IP address.
///   uint16_t    | port        | TCP port.
///   set<string> | expected_ifs | Optional; Interface of typed remote actor.
///
/// - `DELETE` removes either all `port <-> actor` mappings for an actor or only
///   a single one if the optional `port` parameter is set.
///   Type        | Name        | Parameter Description
///   ------------|-------------|------------------------------------------------
///   delete_atom |             | Identifies `DELETE` operations.
///   actor_addr  | whom        | Published actor.
///   uint16_t    | port        | Optional; remove only a single mapping.
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

/// Returns a handle for asynchronous networking operations.
middleman_actor get_middleman_actor(); // implemented in middleman.cpp

} // namespace io
} // namespace caf

#endif // CAF_IO_MIDDLEMAN_ACTOR_HPP

