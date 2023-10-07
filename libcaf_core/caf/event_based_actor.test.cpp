// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/event_based_actor.hpp"

#include "caf/test/caf_test_main.hpp"
#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/event_based_actor.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/send.hpp"

using namespace caf;

WITH_FIXTURE(test::fixture::deterministic) {

TEST("unexpected messages result in an error by default") {
  auto receiver = sys.spawn([](event_based_actor*) -> behavior {
    return {
      [](int32_t x) { return x + 1; },
    };
  });
  auto sender1 = sys.spawn([receiver](event_based_actor* self) -> behavior {
    self->send(receiver, "hello world");
    return {
      [](int32_t) {},
    };
  });
  expect<std::string>().from(sender1).to(receiver);
  expect<error>().with(sec::unexpected_message).from(receiver).to(sender1);
  // Receivers continue receiving messages after unexpected messages.
  auto sender2 = sys.spawn([receiver](event_based_actor* self) -> behavior {
    self->send(receiver, 2);
    return {
      [](int32_t) {},
    };
  });
  expect<int32_t>().with(2).from(sender2).to(receiver);
  expect<int32_t>().with(3).from(receiver).to(sender2);
}

TEST("GH-1589 regression") {
  auto dummy2 = sys.spawn([](event_based_actor* self) {
    self->set_default_handler(skip);
    return behavior{[](int) {}};
  });
  inject().with(dummy2.address()).to(dummy2);
  // No crash means success.
}

} // WITH_FIXTURE(test::fixture::deterministic)

CAF_TEST_MAIN()
