// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE flow.never

#include "caf/flow/observable_builder.hpp"

#include "core-test.hpp"

#include "caf/flow/scoped_coordinator.hpp"

using namespace caf;

namespace {

struct fixture : test_coordinator_fixture<> {
  flow::scoped_coordinator_ptr ctx = flow::make_scoped_coordinator();
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

SCENARIO("the never operator never invokes callbacks except when disposed") {
  GIVEN("a never<int32>") {
    WHEN("an observer subscribes") {
      THEN("the observer never receives any events") {
        auto uut = ctx->make_observable().never<int32_t>();
        auto snk = flow::make_auto_observer<int32_t>();
        uut.subscribe(snk->as_observer());
        ctx->run();
        CHECK(snk->buf.empty());
        CHECK_EQ(snk->state, flow::observer_state::subscribed);
      }
    }
  }
  GIVEN("a never<int32> that gets disposed") {
    WHEN("an observer subscribes") {
      THEN("the observer receives on_complete") {
        auto uut = ctx->make_observable().never<int32_t>();
        auto snk1 = flow::make_auto_observer<int32_t>();
        auto snk2 = flow::make_auto_observer<int32_t>();
        auto sub = uut.subscribe(snk1->as_observer());
        ctx->run();
        CHECK(snk1->buf.empty());
        CHECK_EQ(snk1->state, flow::observer_state::subscribed);
        sub.dispose();
        ctx->run();
        CHECK_EQ(snk1->state, flow::observer_state::completed);
        MESSAGE("dispose only affects the subscription, "
                "the never operator remains unchanged");
        uut.subscribe(snk2->as_observer());
        CHECK_EQ(snk2->state, flow::observer_state::subscribed);
      }
    }
  }
}

END_FIXTURE_SCOPE()
