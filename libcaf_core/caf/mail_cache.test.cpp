// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/mail_cache.hpp"

#include "caf/test/test.hpp"

#include "caf/actor_from_state.hpp"
#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/detail/build_config.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/scoped_actor.hpp"

using namespace caf;
using namespace std::literals;

namespace {

// Switches behaviors to have an explicit "ready" transition.
struct testee_state1 {
  explicit testee_state1(event_based_actor* self) : self(self), cache(self, 5) {
    // nop
  }

  behavior make_behavior() {
    return {
      [this](ok_atom) {
        self->become(behavior{
          [this](get_atom) { return value; },
          [this](add_atom, int increment) { value += increment; },
        });
        cache.unstash();
      },
      [this](message msg) { cache.stash(std::move(msg)); },
    };
  }

  event_based_actor* self;
  mail_cache cache;
  int value = 0;
};

struct fixture {
  actor_system_config cfg;
  actor_system sys;
  scoped_actor self;

  fixture() : sys(init(cfg)), self(sys) {
    // nop
  }

  static actor_system_config& init(actor_system_config& cfg) {
    cfg.set("caf.scheduler.max-threads", 2);
    return cfg;
  }
};

// Uses an "initialized" flag instead of behavior switching.
struct testee_state2 {
  explicit testee_state2(event_based_actor* self) : self(self), cache(self, 5) {
    // nop
  }

  behavior make_behavior() {
    return {
      [this](ok_atom) {
        if (!initialized) {
          initialized = true;
          cache.unstash();
        }
      },
      [this](get_atom) -> result<int> {
        if (!initialized) {
          return cache.stash<int>();
        }
        return value;
      },
      [this](add_atom, int increment) {
        if (!initialized) {
          cache.stash();
          return;
        }
        value += increment;
      },
    };
  }

  bool initialized = false;
  event_based_actor* self;
  mail_cache cache;
  int value = 0;
};

} // namespace

WITH_FIXTURE(fixture) {

TEST("a mail cache can buffer messages until the actor is ready") {
  SECTION("behavior switching") {
    auto aut = sys.spawn(actor_from_state<testee_state1>);
    self->mail(add_atom_v, 1).send(aut);
    self->mail(add_atom_v, 2).send(aut);
    self->mail(add_atom_v, 3).send(aut);
    self->mail(ok_atom_v).send(aut);
    self->mail(add_atom_v, 4).send(aut);
    auto res = self->mail(get_atom_v).request(aut, 1s).receive<int>();
    check_eq(res, 10);
  }
  SECTION("initialized flag") {
    auto aut = sys.spawn(actor_from_state<testee_state2>);
    self->mail(add_atom_v, 1).send(aut);
    self->mail(add_atom_v, 2).send(aut);
    self->mail(add_atom_v, 3).send(aut);
    self->mail(ok_atom_v).send(aut);
    self->mail(add_atom_v, 4).send(aut);
    auto res = self->mail(get_atom_v).request(aut, 1s).receive<int>();
    check_eq(res, 10);
  }
}

#ifdef CAF_ENABLE_EXCEPTIONS
TEST("a mail cache throws when exceeding its capacity") {
  auto aut = sys.spawn(actor_from_state<testee_state1>);
  self->mail(add_atom_v, 1).send(aut);
  self->mail(add_atom_v, 2).send(aut);
  self->mail(add_atom_v, 3).send(aut);
  self->mail(add_atom_v, 4).send(aut);
  self->mail(add_atom_v, 5).send(aut);
  self->mail(add_atom_v, 6).send(aut);
  auto res = self->mail(get_atom_v).request(aut, 1s).receive<int>();
  check_eq(res, make_error(sec::runtime_error));
}
#endif // CAF_ENABLE_EXCEPTIONS

} // WITH_FIXTURE(fixture)
