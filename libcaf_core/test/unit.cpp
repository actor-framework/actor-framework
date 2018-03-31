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
#include "caf/test/unit_test.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/atom.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/unit.hpp"

using namespace caf;

using unit_res_atom   = atom_constant<atom("unitRes")>;
using void_res_atom   = atom_constant<atom("voidRes")>;
using unit_raw_atom   = atom_constant<atom("unitRaw")>;
using void_raw_atom   = atom_constant<atom("voidRaw")>;
using typed_unit_atom = atom_constant<atom("typedUnit")>;

behavior testee(event_based_actor* self) {
  return {
    [] (unit_res_atom)   -> result<unit_t> { return unit; },
    [] (void_res_atom)   -> result<void>   { return {}; },
    [] (unit_raw_atom)   -> unit_t         { return unit; },
    [] (void_raw_atom)   -> void           { },
    [=](typed_unit_atom) -> result<unit_t> {
      auto rp = self->make_response_promise<unit_t>();
      rp.deliver(unit);
      return rp;
    }
  };
}

CAF_TEST(unit_results) {
  actor_system_config cfg;
  actor_system sys{cfg};
  scoped_actor self{sys};
  auto aut = sys.spawn(testee);
  atom_value as[] = {unit_res_atom::value, void_res_atom::value,
                     unit_raw_atom::value, void_raw_atom::value,
                     typed_unit_atom::value};
  for (auto a : as) {
    self->request(aut, infinite, a).receive(
      [&] {
        CAF_MESSAGE("actor under test correctly replied to " << to_string(a));
      },
      [&] (const error&) {
        CAF_FAIL("actor under test failed at input " << to_string(a));
      }
    );
  }
}
