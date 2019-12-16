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

#define CAF_SUITE unit

#include "caf/unit.hpp"

#include "caf/test/unit_test.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/scoped_actor.hpp"

using namespace caf;

behavior testee(event_based_actor* self) {
  return {
    [](add_atom) -> result<unit_t> { return unit; },
    [](get_atom) -> result<void> { return {}; },
    [](put_atom) -> unit_t { return unit; },
    [](resolve_atom) -> void {},
    [=](update_atom) -> result<unit_t> {
      auto rp = self->make_response_promise<unit_t>();
      rp.deliver(unit);
      return rp;
    },
  };
}

CAF_TEST(unit_results) {
  actor_system_config cfg;
  actor_system sys{cfg};
  scoped_actor self{sys};
  auto aut = sys.spawn(testee);
  message as[] = {
    make_message(add_atom_v),    make_message(get_atom_v),
    make_message(put_atom_v),    make_message(resolve_atom_v),
    make_message(update_atom_v),
  };
  for (auto a : as) {
    self->request(aut, infinite, a)
      .receive(
        [&] {
          CAF_MESSAGE("actor under test correctly replied to " << to_string(a));
        },
        [&](const error&) {
          CAF_FAIL("actor under test failed at input " << to_string(a));
        });
  }
}
