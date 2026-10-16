// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/flow/op/interval.hpp"

#include "caf/test/caf_test_main.hpp"
#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/fixture/flow.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/event_based_actor.hpp"

using namespace std::literals;

using namespace caf;

namespace {

using i64_list = std::vector<int64_t>;

struct fixture : test::fixture::deterministic, test::fixture::flow {};

} // namespace

WITH_FIXTURE(fixture) {

SCENARIO("scoped coordinators wait on observable intervals") {
  GIVEN("an observable interval") {
    WHEN("an observer subscribes to it") {
      THEN("the coordinator blocks the current thread for the delays") {
        auto outputs = i64_list{};
        coordinator()
          ->make_observable()
          .interval(50ms, 25ms)
          .take(3)
          .for_each([&outputs](int64_t x) { outputs.emplace_back(x); });
        run_flows();
        check_eq(outputs, i64_list({0, 1, 2}));
      }
    }
  }
}

SCENARIO("scheduled actors schedule observable intervals on the actor clock") {
  GIVEN("an observable interval") {
    WHEN("an observer subscribes to it") {
      THEN("the actor uses the actor clock to schedule flow processing") {
        auto outputs = i64_list{};
        sys.spawn([&outputs](event_based_actor* self) {
          self->make_observable()
            .interval(50ms, 25ms)
            .take(3)
            .for_each([&outputs](int64_t x) { outputs.emplace_back(x); });
        });
        check_eq(mail_count(), 0u);
        dispatch_messages();
        advance_time(40ms);
        dispatch_messages();
        check_eq(outputs, i64_list());
        advance_time(10ms);
        dispatch_messages();
        check_eq(outputs, i64_list({0}));
        advance_time(20ms);
        dispatch_messages();
        check_eq(outputs, i64_list({0}));
        advance_time(10ms);
        dispatch_messages();
        check_eq(outputs, i64_list({0, 1}));
        advance_time(20ms);
        dispatch_messages();
        check_eq(outputs, i64_list({0, 1, 2}));
        run_flows();
        check_eq(outputs, i64_list({0, 1, 2}));
      }
    }
  }
}

SCENARIO("a timer is an observable interval with a single value") {
  GIVEN("an observable timer") {
    WHEN("an observer subscribes to it") {
      THEN("the coordinator observes a single value") {
        auto outputs = i64_list{};
        coordinator()
          ->make_observable()
          .timer(10ms) //
          .for_each([&outputs](int64_t x) { outputs.emplace_back(x); });
        run_flows();
        check_eq(outputs, i64_list({0}));
      }
    }
  }
}

SCENARIO("requesting zero items from an interval is a no-op") {
  GIVEN("an observable interval with a passive observer") {
    WHEN("the observer signals zero demand") {
      THEN("the interval neither arms its timer nor emits items") {
        auto snk = make_passive_observer<int64_t>();
        coordinator()->make_observable().interval(1ms, 1ms).subscribe(
          snk->as_observer());
        run_flows(1ms);
        require(snk->subscribed());
        auto pending = pending_actions();
        snk->sub.request(0);
        check_eq(pending_actions(), pending);
        run_flows(50ms);
        check(snk->buf.empty());
        // Zero demand must not put the interval into a state where it stops
        // reacting to actual demand.
        snk->sub.request(2);
        run_flows(50ms);
        check_eq(snk->buf, i64_list({0, 1}));
        run_flows(50ms);
        check_eq(snk->buf, i64_list({0, 1}));
        snk->unsubscribe();
        run_flows(1ms);
      }
    }
  }
}

SCENARIO("an interval must have a positive period") {
  GIVEN("an observable interval") {
    WHEN("an observer subscribes to it with a negative period") {
      THEN("the coordinator fails the observable") {
        auto outputs = i64_list{};
        auto err = std::make_shared<error>();
        auto obs
          = coordinator()
              ->make_observable()
              .interval(50ms, -25ms)
              .take(3)
              .do_on_error([err](const error& what) { *err = std::move(what); })
              .for_each([&outputs](int64_t x) { outputs.emplace_back(x); });
        run_flows();
        check(outputs.empty());
        check_eq(*err, sec::invalid_argument);
      }
    }
  }
}

} // WITH_FIXTURE(fixture)
