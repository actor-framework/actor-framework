// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE flow.op.never

#include "caf/flow/op/never.hpp"

#include "caf/flow/observable_builder.hpp"
#include "caf/flow/scoped_coordinator.hpp"

#include "core-test.hpp"

using namespace caf;

namespace {

struct fixture : test_coordinator_fixture<> {
  flow::scoped_coordinator_ptr ctx = flow::make_scoped_coordinator();
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

SCENARIO("the never operator never invokes callbacks except when disposed") {
  GIVEN("a never operator") {
    WHEN("an observer subscribes and disposing the subscription") {
      THEN("the observer receives on_complete") {
        using snk_t = flow::auto_observer<int32_t>;
        auto uut = ctx->make_observable().never<int32_t>();
        auto snk1 = ctx->add_child(std::in_place_type<snk_t>);
        auto snk2 = ctx->add_child(std::in_place_type<snk_t>);
        auto sub1 = uut.subscribe(snk1->as_observer());
        ctx->run();
        CHECK(snk1->buf.empty());
        CHECK_EQ(snk1->state, flow::observer_state::subscribed);
        sub1.ptr()->dispose();
        ctx->run();
        CHECK(sub1.ptr()->disposed());
        CHECK_EQ(snk1->state, flow::observer_state::completed);
        MESSAGE("dispose only affects the subscription, "
                "the never operator remains unchanged");
        auto sub2 = uut.subscribe(snk2->as_observer());
        ctx->run();
        CHECK_EQ(snk2->state, flow::observer_state::subscribed);
        sub2.dispose();
        ctx->run();
      }
    }
  }
}

END_FIXTURE_SCOPE()
