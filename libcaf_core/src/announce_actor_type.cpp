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

#include "caf/experimental/announce_actor_type.hpp"

#include "caf/atom.hpp"
#include "caf/send.hpp"
#include "caf/spawn.hpp"
#include "caf/to_string.hpp"
#include "caf/stateful_actor.hpp"
#include "caf/event_based_actor.hpp"

#include "caf/detail/logging.hpp"
#include "caf/detail/singletons.hpp"
#include "caf/detail/actor_registry.hpp"

namespace caf {
namespace experimental {

namespace {

struct spawner_state {
  std::unordered_map<std::string, spawn_fun> funs_;
};

behavior announce_actor_type_server(stateful_actor<spawner_state>* self) {
  return {
    [=](add_atom, std::string& name, spawn_fun& f) {
      self->state.funs_.emplace(std::move(name), std::move(f));
    },
    [=](get_atom, const std::string& name, message& args)
    -> either<ok_atom, actor_addr, std::set<std::string>>
       ::or_else<error_atom, std::string> {
      auto i = self->state.funs_.find(name);
      if (i == self->state.funs_.end())
        return {error_atom::value, "no actor type found named " + name};
      auto f = i->second;
      auto res = f(args);
      if (res.first == invalid_actor_addr)
        return {error_atom::value, "cannot initialize an actor type " + name
                                   + " using the provided arguments"};
      return {ok_atom::value, res.first, res.second};
    },
    others >> [=] {
      CAF_LOGF_WARNING("Unexpected message: "
                       << to_string(self->current_message()));
    }
  };
}

} // namespace <anonymous>

actor spawn_announce_actor_type_server() {
  return spawn<hidden + lazy_init>(announce_actor_type_server);
}

void announce_actor_factory(std::string name, spawn_fun f) {
  auto registry = detail::singletons::get_actor_registry();
  auto server = registry->get_named(atom("SpawnServ"));
  anon_send(server, add_atom::value, std::move(name), std::move(f));
}

} // namespace experimental
} // namespace caf
