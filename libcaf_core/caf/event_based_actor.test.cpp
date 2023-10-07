// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/event_based_actor.hpp"

#include "caf/test/caf_test_main.hpp"
#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/scenario.hpp"
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

SCENARIO("request(...).await(...) suspends the regular actor behavior") {
  GIVEN("an actor that responds to integers in its regular behavior") {
    auto count = std::make_shared<int>(0);
    auto server_impl = []() -> behavior { return {[](int x) { return x; }}; };
    auto uut_impl = [count](event_based_actor* self, actor buddy) {
      self->request(buddy, infinite, 42).await([](int) {});
      return behavior{
        [count](int x) {
          *count += x;
          return *count;
        },
      };
    };
    WHEN("the actor receives an integer while waiting for the response") {
      THEN("the actor skips the message until the response arrives") {
        auto server = sys.spawn(server_impl);
        auto aut = sys.spawn(uut_impl, server);
        inject().with(23).to(aut); // skipped
        check_eq(*count, 0);
        expect<int>().with(42).from(aut).to(server);
        expect<int>().with(42).from(server).to(aut);
        expect<int>().with(23).to(aut); // put back to the mailbox after await
        check_eq(*count, 23);
      }
    }
    WHEN("the actor receives an exit_msg while waiting for the response") {
      THEN("the actor processes the exit messages immediately") {
        auto server = sys.spawn(server_impl);
        printf("server is %d\n", (int) server.id());
        auto aut = sys.spawn(uut_impl, server);
        printf("aut is %d\n", (int) aut.id());
        inject().with(exit_msg{server.address(), exit_reason::kill}).to(aut);
        check(terminated(aut));
        expect<int>().with(42).from(aut).to(server);
      }
    }
  }
}

} // WITH_FIXTURE(test::fixture::deterministic)

CAF_TEST_MAIN()
