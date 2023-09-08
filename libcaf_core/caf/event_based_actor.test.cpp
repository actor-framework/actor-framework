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
      [](int32_t) {},
    };
  });
  auto sender = sys.spawn([receiver](event_based_actor* self) -> behavior {
    self->send(receiver, "hello world");
    return {
      [](int32_t) {},
    };
  });
  expect<std::string>().from(sender).to(receiver);
  expect<error>().with(sec::unexpected_message).from(receiver).to(sender);
}

} // WITH_FIXTURE(test::fixture::deterministic)

CAF_TEST_MAIN()
