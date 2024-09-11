// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/unit.hpp"

#include "caf/test/test.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/log/test.hpp"
#include "caf/scoped_actor.hpp"

using namespace caf;

TEST("unit_results") {
  auto testee = [](event_based_actor* self) -> behavior {
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
  };
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
    self->mail(a)
      .request(aut, infinite)
      .receive(
        [&a] {
          log::test::debug("actor under test correctly replied to {}",
                           to_string(a));
        },
        [this, &a](const error&) {
          fail("actor under test failed at input {}", to_string(a));
        });
  }
}

TEST("actor_address") {
  actor_system_config cfg;
  actor_system sys{cfg};
  scoped_actor self{sys};
  check_ne(self.address().id(), 0u);
}
