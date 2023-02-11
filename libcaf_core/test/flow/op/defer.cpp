// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE flow.op.defer

#include "caf/flow/op/defer.hpp"

#include "core-test.hpp"

#include "caf/flow/observable_builder.hpp"
#include "caf/flow/scoped_coordinator.hpp"

using namespace caf;

namespace {

struct fixture : test_coordinator_fixture<> {
  flow::scoped_coordinator_ptr ctx = flow::make_scoped_coordinator();
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

SCENARIO("the defer operator produces a fresh observable for each observer") {
  GIVEN("a deferred observable") {
    WHEN("two observers subscribes") {
      THEN("each observer subscribes to a fresh observable") {
        using ivec = std::vector<int>;
        size_t factory_calls = 0;
        auto factory = [this, &factory_calls] {
          ++factory_calls;
          return ctx->make_observable().iota(1).take(5);
        };
        auto uut = ctx->make_observable().defer(factory);
        auto snk1 = flow::make_passive_observer<int>();
        auto snk2 = flow::make_passive_observer<int>();
        uut.subscribe(snk1->as_observer());
        CHECK_EQ(factory_calls, 1u);
        REQUIRE(snk1->sub);
        uut.subscribe(snk2->as_observer());
        CHECK_EQ(factory_calls, 2u);
        REQUIRE(snk2->sub);
        snk1->sub.request(27);
        snk2->sub.request(3);
        ctx->run();
        CHECK_EQ(snk1->state, flow::observer_state::completed);
        CHECK_EQ(snk1->buf, ivec({1, 2, 3, 4, 5}));
        CHECK_EQ(snk2->state, flow::observer_state::subscribed);
        CHECK_EQ(snk2->buf, ivec({1, 2, 3}));
        snk2->sub.request(2);
        ctx->run();
        CHECK_EQ(snk2->state, flow::observer_state::completed);
        CHECK_EQ(snk2->buf, ivec({1, 2, 3, 4, 5}));
      }
    }
  }
}

END_FIXTURE_SCOPE()
