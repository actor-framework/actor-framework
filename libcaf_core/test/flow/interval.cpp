// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE flow.interval

#include "caf/flow/observable_builder.hpp"

#include "core-test.hpp"

#include "caf/flow/scoped_coordinator.hpp"

using namespace std::literals;

using namespace caf;

using i64_list = std::vector<int64_t>;

namespace {

struct fixture : test_coordinator_fixture<> {
  flow::scoped_coordinator_ptr ctx = flow::make_scoped_coordinator();
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(interval_tests, fixture)

SCENARIO("scoped coordinators wait on observable intervals") {
  GIVEN("an observable interval") {
    WHEN("an observer subscribes to it") {
      THEN("the coordinator blocks the current thread for the delays") {
        auto outputs = i64_list{};
        ctx->make_observable()
          .interval(50ms, 25ms)
          .take(3)
          .for_each([&outputs](int64_t x) { outputs.emplace_back(x); });
        ctx->run();
        CHECK_EQ(outputs, i64_list({0, 1, 2}));
      }
    }
  }
}

SCENARIO("scheduled actors schedule observable intervals on the actor clock") {
  GIVEN("an observable interval") {
    WHEN("an observer subscribes to it") {
      THEN("the actor uses the actor clock to schedule flow processing") {
        auto outputs = i64_list{};
        sys.spawn([&outputs](caf::event_based_actor* self) {
          self->make_observable()
            .interval(50ms, 25ms)
            .take(3)
            .for_each([&outputs](int64_t x) { outputs.emplace_back(x); });
        });
        CHECK(sched.clock().actions.empty());
        sched.run();
        CHECK_EQ(sched.clock().actions.size(), 1u);
        advance_time(40ms);
        sched.run();
        CHECK_EQ(outputs, i64_list());
        advance_time(10ms);
        sched.run();
        CHECK_EQ(outputs, i64_list({0}));
        advance_time(20ms);
        sched.run();
        CHECK_EQ(outputs, i64_list({0}));
        advance_time(10ms);
        sched.run();
        CHECK_EQ(outputs, i64_list({0, 1}));
        advance_time(20ms);
        sched.run();
        CHECK_EQ(outputs, i64_list({0, 1, 2}));
        run();
        CHECK_EQ(outputs, i64_list({0, 1, 2}));
      }
    }
  }
}

SCENARIO("a timer is an observable interval with a single value") {
  GIVEN("an observable timer") {
    WHEN("an observer subscribes to it") {
      THEN("the coordinator observes a single value") {
        auto outputs = i64_list{};
        ctx->make_observable()
          .timer(10ms) //
          .for_each([&outputs](int64_t x) { outputs.emplace_back(x); });
        ctx->run();
        CHECK_EQ(outputs, i64_list({0}));
      }
    }
  }
}

CAF_TEST_FIXTURE_SCOPE_END()
