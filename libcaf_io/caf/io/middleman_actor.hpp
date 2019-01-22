/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include "caf/fwd.hpp"
#include "caf/atom.hpp"
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
///   // port: Unused TCP port or 0 for any.
///   // whom: Actor that should be published at given port.
///   // ifs: Interface of given actor.
///   // addr: IP address to listen to or empty for any.
///   // reuse:_addr: Enables or disables SO_REUSEPORT option.
///   (publish_atom, uint16_t port, strong_actor_ptr whom,
///    set<string> ifs, string addr, bool reuse_addr)
///   -> (uint16_t)
///
///   // Opens a new port other CAF instances can connect to. The
///   // difference between `PUBLISH` and `OPEN` is that no actor is mapped to
///   // this port, meaning that connecting nodes only get a valid `node_id`
///   // handle when connecting.
///   // port: Unused TCP port or 0 for any.
///   // addr: IP address to listen to or empty for any.
///   // reuse:_addr: Enables or disables SO_REUSEPORT option.
///   (open_atom, uint16_t port, string addr, bool reuse_addr)
///   -> (uint16_t)
///
///   // Queries a remote node and returns an ID to this node as well as
///   // an `strong_actor_ptr` to a remote actor if an actor was published at this
///   // port. The actor address must be cast to either `actor` or
///   // `typed_actor` using `actor_cast` after validating `ifs`.
///   // hostname: IP address or DNS hostname.
///   // port: TCP port.
///   (connect_atom, string hostname, uint16_t port)
///   -> (node_id nid, strong_actor_ptr remote_actor, set<string> ifs)
///
///   // Closes `port` if it is mapped to `whom`.
///   // whom: A published actor.
///   // port: Used TCP port.
///   (unpublish_atom, strong_actor_ptr whom, uint16_t port)
///   -> void
///
///   // Unconditionally closes `port`, removing any actor
///   // published at this port.
///   // port: Used TCP port.
///   (close_atom, uint16_t port)
///   -> void
///
///   // Spawns an actor on a remote node, initializing it using the arguments
///   // stored in `msg` and returns the address of the spawned actor and its
///   // interface description on success; an error string otherwise.
///   // nid: ID of the remote node that should spawn the actor.
///   // name: Announced type name of the actor.
///   // args: Initialization arguments for the actor.
///   (spawn_atom, node_id nid, string name, message args)
///   -> (strong_actor_ptr, set<string>)
///
/// }
/// ~~~
using middleman_actor =
  typed_actor<
    replies_to<publish_atom, uint16_t, strong_actor_ptr,
               std::set<std::string>, std::string, bool>
    ::with<uint16_t>,


    replies_to<open_atom, uint16_t, std::string, bool>
    ::with<uint16_t>,

    replies_to<connect_atom, std::string, uint16_t>
    ::with<node_id, strong_actor_ptr, std::set<std::string>>,

    reacts_to<unpublish_atom, actor_addr, uint16_t>,

    reacts_to<close_atom, uint16_t>,

    replies_to<spawn_atom, node_id, std::string, message, std::set<std::string>>
    ::with<strong_actor_ptr>,

    replies_to<get_atom, node_id>::with<node_id, std::string, uint16_t>>;

/// @relates middleman_actor
middleman_actor make_middleman_actor(actor_system& sys, actor db);

} // namespace io
} // namespace caf

