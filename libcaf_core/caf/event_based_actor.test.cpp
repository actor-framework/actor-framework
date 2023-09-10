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
      [](int32_t x) { return ++x; },
    };
  });
  auto sender = sys.spawn([receiver](event_based_actor* self) -> behavior {
    self->send(receiver, "hello world");
    return {
      [](int32_t) {},
    };
  });
  expect<std::string>().from(sender).to(receiver);
  SECTION("receiver sends an unexpected_message error to sender") {
    expect<error>().with(sec::unexpected_message).from(receiver).to(sender);
  }
  SECTION("receiver continues to receive message after returning error") {
    expect<error>().with(sec::unexpected_message).from(receiver).to(sender);
    auto integer_sender
      = sys.spawn([receiver](event_based_actor* self) -> behavior {
          self->send(receiver, 2);
          return {
            [](int32_t) {},
          };
        });
    expect<int32_t>().with(2).from(integer_sender).to(receiver);
    expect<int32_t>().with(3).from(receiver).to(integer_sender);
  }
}

} // WITH_FIXTURE(test::fixture::deterministic)

CAF_TEST_MAIN()
