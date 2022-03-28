// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE flow.error

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

SCENARIO("an error observable immediately calls on_error on any subscriber") {
  GIVEN("an error<int32>") {
    WHEN("an observer subscribes") {
      THEN("the observer receives on_error") {
        auto uut = ctx->make_observable().error<int32_t>(sec::runtime_error);
        auto snk = flow::make_passive_observer<int32_t>();
        uut.subscribe(snk->as_observer());
        ctx->run();
        CHECK(!snk->sub);
        CHECK_EQ(snk->state, flow::observer_state::aborted);
        CHECK(snk->buf.empty());
      }
    }
  }
}

END_FIXTURE_SCOPE()
