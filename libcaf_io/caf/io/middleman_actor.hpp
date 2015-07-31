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
/// interface middleman_actor {
///
///   // Establishes a new `port <-> actor` mapping and  returns the actual
///   // port in use on success. Passing 0 as port instructs the OS to choose
///   // the next high-level port available for binding.
///   // @param port: Unused TCP port or 0 for any.
///   // @param whom: Actor that should be published at given port.
///   // @param ifs: Interface of given actor.
///   // @param addr: IP address to listen to or empty for any.
///   // @param reuse_addr: Enables or disables SO_REUSEPORT option.
///   (publish_atom, uint16_t port, actor_addr whom,
///    set<string> ifs, string addr, bool reuse_addr)
///   -> either (ok_atom, uint16_t port)
///      or     (error_atom, string error_string)
///
///   // Opens a new port other CAF instances can connect to. The
///   // difference between `PUBLISH` and `OPEN` is that no actor is mapped to
///   // this port, meaning that connecting nodes only get a valid `node_id`
///   // handle when connecting.
///   // @param port: Unused TCP port or 0 for any.
///   // @param addr: IP address to listen to or empty for any.
///   // @param reuse_addr: Enables or disables SO_REUSEPORT option.
///   (open_atom, uint16_t port, string addr, bool reuse_addr)
///   -> either (ok_atom, uint16_t port)
///      or     (error_atom, string error_string)
///
///   // Queries a remote node and returns an ID to this node as well as
///   // an `actor_addr` to a remote actor if an actor was published at this
///   // port. The actor address must be cast to either `actor` or
///   // `typed_actor` using `actor_cast` after validating `ifs`.
///   // @param hostname IP address or DNS hostname.
///   // @param port TCP port.
///   (connect_atom, string hostname, uint16_t port)
///   -> either (ok_atom, node_id nid, actor_addr remote_actor, set<string> ifs)
///      or     (error_atom, string error_string)
///
///   // Closes `port` if it is mapped to `whom`.
///   // @param whom A published actor.
///   // @param port Used TCP port.
///   (unpublish_atom, actor_addr whom, uint16_t port)
///   -> either (ok_atom)
///      or     (error_atom, string error_string)
///
///   // Unconditionally closes `port`, removing any actor
///   // published at this port.
///   // @param port Used TCP port.
///   (close_atom, uint16_t port)
///   -> either (ok_atom)
///      or     (error_atom, string error_string)
///
/// }
/// ~~~
using middleman_actor =
  typed_actor<
    replies_to<publish_atom, uint16_t, actor_addr,
               std::set<std::string>, std::string, bool>
    ::with_either<ok_atom, uint16_t>
    ::or_else<error_atom, std::string>,

    replies_to<open_atom, uint16_t, std::string, bool>
    ::with_either<ok_atom, uint16_t>
    ::or_else<error_atom, std::string>,

    replies_to<connect_atom, std::string, uint16_t>
    ::with_either<ok_atom, node_id, actor_addr, std::set<std::string>>
    ::or_else<error_atom, std::string>,

    replies_to<unpublish_atom, actor_addr, uint16_t>
    ::with_either<ok_atom>
    ::or_else<error_atom, std::string>,

    replies_to<close_atom, uint16_t>
    ::with_either<ok_atom>
    ::or_else<error_atom, std::string>>;

/// Returns a handle for asynchronous networking operations.
middleman_actor get_middleman_actor(); // implemented in middleman.cpp

} // namespace io
} // namespace caf

#endif // CAF_IO_MIDDLEMAN_ACTOR_HPP
