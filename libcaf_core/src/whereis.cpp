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

#include "caf/experimental/whereis.hpp"

#include "caf/actor.hpp"
#include "caf/scoped_actor.hpp"

#include "caf/detail/singletons.hpp"
#include "caf/detail/actor_registry.hpp"

namespace caf {
namespace experimental {

actor whereis(atom_value registered_name) {
  return detail::singletons::get_actor_registry()->get_named(registered_name);
}

actor whereis(atom_value registered_name, node_id nid) {
  auto basp = whereis(atom("BASP"));
  if (basp == invalid_actor)
    return invalid_actor;
  actor result;
  scoped_actor self;
  try {
    self->send(basp, forward_atom::value, self.address(), nid, registered_name,
               make_message(sys_atom::value, get_atom::value, "info"));
    self->receive(
      [&](ok_atom, const std::string& /* key == "info" */,
          const actor_addr& addr, const std::string& /* name */) {
        result = actor_cast<actor>(addr);
      },
      after(std::chrono::minutes(5)) >> [] {
        // nop
      }
    );
  } catch (...) {
    // nop
  }

  return result;
}

} // namespace experimental
} // namespace caf
