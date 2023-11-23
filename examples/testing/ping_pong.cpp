
// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/caf_test_main.hpp"
#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/event_based_actor.hpp"

using namespace caf;

// --(rst-ping-pong-begin)--
namespace {

behavior ping(event_based_actor* self, actor pong_actor, int n) {
  self->send(pong_actor, ping_atom_v, n);
  return {
    [=](pong_atom, int x) {
      if (x > 1)
        self->send(pong_actor, ping_atom_v, x - 1);
    },
  };
}

behavior pong() {
  return {
    [=](ping_atom, int x) { return make_result(pong_atom_v, x); },
  };
}

WITH_FIXTURE(caf::test::fixture::deterministic) {

TEST("two actors can communicate with each other") {
  // Spawn the Ping actor and run its initialization code.
  auto pong_actor = sys.spawn(pong);
  auto ping_actor = sys.spawn(ping, pong_actor, 3);
  // Test communication between Ping and Pong.
  expect<ping_atom, int>().with(std::ignore, 3).from(ping_actor).to(pong_actor);
  expect<pong_atom, int>().with(std::ignore, 3).from(pong_actor).to(ping_actor);
  expect<ping_atom, int>().with(std::ignore, 2).from(ping_actor).to(pong_actor);
  expect<pong_atom, int>().with(std::ignore, 2).from(pong_actor).to(ping_actor);
  expect<ping_atom, int>().with(std::ignore, 1).from(ping_actor).to(pong_actor);
  expect<pong_atom, int>().with(std::ignore, 1).from(pong_actor).to(ping_actor);
  check_eq(mail_count(), 0u);
}

} // WITH_FIXTURE(caf::test::fixture::deterministic)

} // namespace
// --(rst-ping-pong-end)--

CAF_TEST_MAIN()
