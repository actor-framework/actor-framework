// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE flow.generation

#include "caf/flow/observable.hpp"

#include "core-test.hpp"

#include "caf/flow/coordinator.hpp"
#include "caf/flow/merge.hpp"
#include "caf/flow/observable_builder.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/scoped_coordinator.hpp"

using namespace caf;

namespace {

struct fixture : test_coordinator_fixture<> {
  flow::scoped_coordinator_ptr ctx = flow::make_scoped_coordinator();
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

SCENARIO("the repeater source repeats one value indefinitely") {
  GIVEN("a repeater source") {
    WHEN("subscribing to its output") {
      THEN("the observer receives the same value over and over again") {
        using ivec = std::vector<int>;
        auto snk = flow::make_passive_observer<int>();
        ctx->make_observable().repeat(42).subscribe(snk->as_observer());
        CHECK_EQ(snk->state, flow::observer_state::subscribed);
        CHECK(snk->buf.empty());
        if (CHECK(snk->sub)) {
          snk->sub.request(3);
          ctx->run();
          CHECK_EQ(snk->buf, ivec({42, 42, 42}));
          snk->sub.request(4);
          ctx->run();
          CHECK_EQ(snk->buf, ivec({42, 42, 42, 42, 42, 42, 42}));
          snk->sub.cancel();
          ctx->run();
          CHECK_EQ(snk->buf, ivec({42, 42, 42, 42, 42, 42, 42}));
        }
      }
    }
  }
}

SCENARIO("the container source streams its input values") {
  GIVEN("a container source") {
    WHEN("subscribing to its output") {
      THEN("the observer receives the values from the container in order") {
        using ivec = std::vector<int>;
        auto xs = ivec{1, 2, 3, 4, 5, 6, 7};
        auto snk = flow::make_passive_observer<int>();
        ctx->make_observable().from_container(xs).subscribe(snk->as_observer());
        CHECK_EQ(snk->state, flow::observer_state::subscribed);
        CHECK(snk->buf.empty());
        if (CHECK(snk->sub)) {
          snk->sub.request(3);
          ctx->run();
          CHECK_EQ(snk->buf, ivec({1, 2, 3}));
          snk->sub.request(21);
          ctx->run();
          CHECK_EQ(snk->buf, ivec({1, 2, 3, 4, 5, 6, 7}));
          CHECK_EQ(snk->state, flow::observer_state::completed);
        }
      }
    }
  }
}

END_FIXTURE_SCOPE()
