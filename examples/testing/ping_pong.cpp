// Manual refs: lines 12-65 (Testing)

#define CAF_SUITE ping_pong

#include "caf/test/dsl.hpp"
#include "caf/test/unit_test_impl.hpp"

#include "caf/all.hpp"

using namespace caf;

namespace {

using ping_atom = atom_constant<atom("ping")>;
using pong_atom = atom_constant<atom("pong")>;

behavior ping(event_based_actor* self, actor pong_actor, int n) {
  self->send(pong_actor, ping_atom::value, n);
  return {
    [=](pong_atom, int x) {
      if (x > 1)
        self->send(pong_actor, ping_atom::value, x - 1);
    }
  };
}

behavior pong() {
  return {
    [=](ping_atom, int x) {
      return std::make_tuple(pong_atom::value, x);
    }
  };
}

struct ping_pong_fixture : test_coordinator_fixture<> {
  actor pong_actor;

  ping_pong_fixture() {
    // Spawn the Pong actor.
    pong_actor = sys.spawn(pong);
    // Run initialization code for Pong.
    run();
  }
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(ping_pong_tests, ping_pong_fixture)

CAF_TEST(three pings) {
  // Spawn the Ping actor and run its initialization code.
  auto ping_actor = sys.spawn(ping, pong_actor, 3);
  sched.run_once();
  // Test communication between Ping and Pong.
  expect((ping_atom, int), from(ping_actor).to(pong_actor).with(_, 3));
  expect((pong_atom, int), from(pong_actor).to(ping_actor).with(_, 3));
  expect((ping_atom, int), from(ping_actor).to(pong_actor).with(_, 2));
  expect((pong_atom, int), from(pong_actor).to(ping_actor).with(_, 2));
  expect((ping_atom, int), from(ping_actor).to(pong_actor).with(_, 1));
  expect((pong_atom, int), from(pong_actor).to(ping_actor).with(_, 1));
  // No further messages allowed.
  disallow((ping_atom, int), from(ping_actor).to(pong_actor).with(_, 1));
}

CAF_TEST_FIXTURE_SCOPE_END()
