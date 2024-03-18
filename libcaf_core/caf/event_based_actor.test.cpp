// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/event_based_actor.hpp"

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/event_based_actor.hpp"
#include "caf/scoped_actor.hpp"

using namespace caf;
using namespace std::literals;

namespace {

WITH_FIXTURE(test::fixture::deterministic) {

TEST("unexpected messages result in an error by default") {
  auto receiver = sys.spawn([](event_based_actor*) -> behavior {
    return {
      [](int32_t x) { return x + 1; },
    };
  });
  auto sender1 = sys.spawn([receiver](event_based_actor* self) -> behavior {
    self->mail("hello world").send(receiver);
    return {
      [](int32_t) {},
    };
  });
  expect<std::string>().from(sender1).to(receiver);
  expect<error>().with(sec::unexpected_message).from(receiver).to(sender1);
  // Receivers continue receiving messages after unexpected messages.
  auto sender2 = sys.spawn([receiver](event_based_actor* self) -> behavior {
    self->mail(2).send(receiver);
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
      self->mail(42).request(buddy, infinite).await([](int) {});
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
        auto aut = sys.spawn(uut_impl, server);
        inject().with(exit_msg{server.address(), exit_reason::kill}).to(aut);
        check(terminated(aut));
        expect<int>().with(42).from(aut).to(server);
      }
    }
  }
}

SCENARIO("idle timeouts trigger after a period of inactivity") {
  GIVEN("an actor with an idle timeout scheduled to trigger once") {
    auto timeout_called = std::make_shared<bool>(false);
    auto aut = sys.spawn([timeout_called](event_based_actor* self) -> behavior {
      self->set_idle_handler(500ms, strong_ref, once,
                             [timeout_called] { *timeout_called = true; });
      return {
        [](const std::string&) {},
      };
    });
    WHEN("a message arrives") {
      THEN("the timeout resets") {
        advance_time(499ms);
        inject().with("hello"s).to(aut);
        check_eq(next_timeout(), sys.clock().now() + 500ms);
      }
    }
    AND_WHEN("no message arrives afterwards") {
      THEN("the timeout triggers") {
        advance_time(500ms);
        expect<timeout_msg>().to(aut);
        check(*timeout_called);
      }
    }
  }
  GIVEN("an actor with an idle timeout scheduled to trigger repeatedly") {
    auto first_handler_calls = std::make_shared<int>(0);
    auto second_handler_calls = std::make_shared<int>(0);
    auto aut = sys.spawn([=](event_based_actor* self) -> behavior {
      self->set_idle_handler(500ms, strong_ref, repeat, [=] {
        if (++*first_handler_calls == 3) {
          self->set_idle_handler(300ms, strong_ref, repeat,
                                 [=] { ++*second_handler_calls; });
        }
      });
      return {
        [](const std::string&) {},
      };
    });
    WHEN("a message arrives") {
      THEN("the timeout resets") {
        advance_time(499ms);
        inject().with("hello"s).to(aut);
        check_eq(next_timeout(), sys.clock().now() + 500ms);
      }
    }
    AND_WHEN("no message arrives afterwards") {
      THEN("the timeout triggers repeatedly") {
        advance_time(500ms);
        expect<timeout_msg>().to(aut);
        check_eq(*first_handler_calls, 1);
        check_eq(*second_handler_calls, 0);
        advance_time(500ms);
        expect<timeout_msg>().to(aut);
        check_eq(*first_handler_calls, 2);
        check_eq(*second_handler_calls, 0);
        advance_time(500ms);
        expect<timeout_msg>().to(aut);
        check_eq(*first_handler_calls, 3);
        check_eq(*second_handler_calls, 0);
        advance_time(300ms);
        expect<timeout_msg>().to(aut);
        check_eq(*first_handler_calls, 3);
        check_eq(*second_handler_calls, 1);
        advance_time(300ms);
        expect<timeout_msg>().to(aut);
        check_eq(*first_handler_calls, 3);
        check_eq(*second_handler_calls, 2);
      }
    }
  }
}

SCENARIO("weak idle timeouts do not prevent actors from becoming unreachable") {
  GIVEN("an actor with a weak idle timeout scheduled to trigger once") {
    WHEN("the actor becomes unreachable") {
      THEN("the timeout does not trigger") {
        auto aut = sys.spawn([](event_based_actor* self) -> behavior {
          self->set_idle_handler(500ms, weak_ref, once, [] {});
          return {
            [](const std::string&) {},
          };
        });
        auto addr = aut.address();
        check(has_pending_timeout());
        check_eq(addr.get()->strong_refs, 1u);
        aut = nullptr;
        check_eq(addr.get()->strong_refs, 0u);
        check(!has_pending_timeout());
      }
    }
  }
  GIVEN("an actor with a weak idle timeout scheduled to trigger repeatedly") {
    WHEN("the actor becomes unreachable") {
      THEN("the timeout does not trigger") {
        auto aut = sys.spawn([](event_based_actor* self) -> behavior {
          self->set_idle_handler(500ms, weak_ref, repeat, [] {});
          return {
            [](const std::string&) {},
          };
        });
        auto addr = aut.address();
        check(has_pending_timeout());
        check_eq(addr.get()->strong_refs, 1u);
        aut = nullptr;
        check_eq(addr.get()->strong_refs, 0u);
        check(!has_pending_timeout());
      }
    }
  }
}

SCENARIO("setting an infinite idle timeout is an error") {
  GIVEN("an actor") {
    WHEN("setting an infinite idle timeout") {
      THEN("the actor terminates with an error") {
        auto aut = sys.spawn([](event_based_actor* self) -> behavior {
          self->set_idle_handler(infinite, strong_ref, once, [] {});
          return {
            [](const std::string&) {},
          };
        });
        auto aut_down = std::make_shared<bool>(false);
        auto observer = sys.spawn([aut, aut_down](event_based_actor* self) {
          self->monitor(aut, [aut_down](const error&) { *aut_down = true; });
          return behavior{
            [=](const std::string&) {},
          };
        });
        dispatch_messages();
        check(*aut_down);
      }
    }
  }
}

} // WITH_FIXTURE(test::fixture::deterministic)

} // namespace
