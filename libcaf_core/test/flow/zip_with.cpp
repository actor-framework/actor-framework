// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE flow.zip_with

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

SCENARIO("zip_with combines inputs") {
  GIVEN("two observables") {
    WHEN("merging them with zip_with") {
      THEN("the observer receives the combined output of both sources") {
        auto snk = flow::make_passive_observer<int>();
        ctx->make_observable()
          .zip_with([](int x, int y) { return x + y; },
                    ctx->make_observable().repeat(11).take(113),
                    ctx->make_observable().repeat(22).take(223))
          .subscribe(snk->as_observer());
        ctx->run();
        REQUIRE_EQ(snk->state, flow::observer_state::subscribed);
        snk->sub.request(64);
        ctx->run();
        CHECK_EQ(snk->state, flow::observer_state::subscribed);
        CHECK_EQ(snk->buf.size(), 64u);
        snk->sub.request(64);
        ctx->run();
        CHECK_EQ(snk->state, flow::observer_state::completed);
        CHECK_EQ(snk->buf.size(), 113u);
        CHECK_EQ(snk->buf, std::vector<int>(113, 33));
      }
    }
  }
}

SCENARIO("zip_with emits nothing when zipping an empty observable") {
  GIVEN("two observables, one of them empty") {
    WHEN("merging them with zip_with") {
      THEN("the observer sees on_complete immediately") {
        auto snk = flow::make_auto_observer<int>();
        ctx->make_observable()
          .zip_with([](int x, int y, int z) { return x + y + z; },
                    ctx->make_observable().repeat(11),
                    ctx->make_observable().repeat(22),
                    ctx->make_observable().empty<int>())
          .subscribe(snk->as_observer());
        ctx->run();
        CHECK(snk->buf.empty());
        CHECK_EQ(snk->state, flow::observer_state::completed);
      }
    }
  }
}

END_FIXTURE_SCOPE()
