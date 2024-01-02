// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/event_based_actor.hpp"

using namespace caf;
using namespace std::literals;

namespace {

behavior dummy_behavior() {
  return {
    [](int) {},
  };
}

WITH_FIXTURE(test::fixture::deterministic) {

SCENARIO("run_scheduled triggers an action after a relative timeout") {
  GIVEN("a scheduled actor") {
    WHEN("the actor schedules an action with run_scheduled") {
      THEN("the action triggers after the relative timeout") {
        auto called = std::make_shared<bool>(false);
        auto aut = sys.spawn([called](event_based_actor* self) {
          auto now = self->clock().now();
          self->run_scheduled(now + 1s, [called] { *called = true; });
          return dummy_behavior();
        });
        dispatch_messages();
        check(!*called);
        advance_time(1s);
        dispatch_messages();
        check(*called);
      }
      AND_THEN("disposing the pending timeout cancels the action") {
        auto called = std::make_shared<bool>(false);
        auto pending = disposable{};
        auto aut = sys.spawn([called, &pending](event_based_actor* self) {
          auto now = self->clock().now();
          pending = self->run_scheduled(now + 1s, [called] { *called = true; });
          return dummy_behavior();
        });
        dispatch_messages();
        check(!*called);
        pending.dispose();
        advance_time(1s);
        dispatch_messages();
        check(!*called);
      }
    }
  }
}

SCENARIO("run_scheduled_weak triggers an action after a relative timeout") {
  GIVEN("a scheduled actor") {
    WHEN("the actor schedules an action with run_scheduled") {
      THEN("the action triggers after the relative timeout for live actors") {
        auto called = std::make_shared<bool>(false);
        auto aut = sys.spawn([called](event_based_actor* self) {
          auto now = self->clock().now();
          self->run_scheduled_weak(now + 1s, [called] { *called = true; });
          return dummy_behavior();
        });
        dispatch_messages();
        check(!*called);
        advance_time(1s);
        dispatch_messages();
        check(*called);
      }
      AND_THEN("no action triggers for terminated actors") {
        auto called = std::make_shared<bool>(false);
        sys.spawn([called](event_based_actor* self) {
          auto now = self->clock().now();
          self->run_scheduled_weak(now + 1s, [called] { *called = true; });
          return dummy_behavior();
        });
        dispatch_messages();
        check(!*called);
        advance_time(1s);
        dispatch_messages();
        check(!*called);
      }
      AND_THEN("disposing the pending timeout cancels the action") {
        auto called = std::make_shared<bool>(false);
        auto pending = disposable{};
        auto aut = sys.spawn([called, &pending](event_based_actor* self) {
          auto now = self->clock().now();
          pending = self->run_scheduled_weak(now + 1s,
                                             [called] { *called = true; });
          return dummy_behavior();
        });
        dispatch_messages();
        check(!*called);
        pending.dispose();
        advance_time(1s);
        dispatch_messages();
        check(!*called);
      }
    }
  }
}

} // WITH_FIXTURE(test::fixture::deterministic)

} // namespace
