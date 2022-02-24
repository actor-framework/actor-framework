// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE flow.zip_with

#include "caf/flow/zip_with.hpp"

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

SCENARIO("zip_with combines inputs") {
  GIVEN("two observables") {
    WHEN("merging them with zip_with") {
      THEN("the observer receives the combined output of both sources") {
        auto o1 = flow::make_passive_observer<int>();
        auto fn = [](int x, int y) { return x + y; };
        auto r1 = ctx->make_observable().repeat(11).take(113);
        auto r2 = ctx->make_observable().repeat(22).take(223);
        flow::zip_with(fn, std::move(r1), std::move(r2))
          .subscribe(o1->as_observer());
        ctx->run();
        REQUIRE_EQ(o1->state, flow::observer_state::subscribed);
        o1->sub.request(64);
        ctx->run();
        CHECK_EQ(o1->state, flow::observer_state::subscribed);
        CHECK_EQ(o1->buf.size(), 64u);
        o1->sub.request(64);
        ctx->run();
        CHECK_EQ(o1->state, flow::observer_state::completed);
        CHECK_EQ(o1->buf.size(), 113u);
        CHECK(std::all_of(o1->buf.begin(), o1->buf.begin(),
                          [](int x) { return x == 33; }));
      }
    }
  }
}

SCENARIO("zip_with emits nothing when zipping an empty observable") {
  GIVEN("two observables, one of them empty") {
    WHEN("merging them with zip_with") {
      THEN("the observer sees on_complete immediately") {
        auto o1 = flow::make_passive_observer<int>();
        auto fn = [](int x, int y, int z) { return x + y + z; };
        auto r1 = ctx->make_observable().repeat(11);
        auto r2 = ctx->make_observable().repeat(22);
        auto r3 = ctx->make_observable().empty<int>();
        flow::zip_with(fn, std::move(r1), std::move(r2), std::move(r3))
          .subscribe(o1->as_observer());
        ctx->run();
        REQUIRE_EQ(o1->state, flow::observer_state::subscribed);
        o1->sub.request(64);
        ctx->run();
        CHECK_EQ(o1->state, flow::observer_state::completed);
        CHECK(o1->buf.empty());
      }
    }
  }
}

SCENARIO("zip_with may only have more than one subscriber") {
  GIVEN("two observables") {
    WHEN("merging them with zip_with") {
      THEN("all observer receives the combined output of both sources") {
        auto o1 = flow::make_passive_observer<int>();
        auto o2 = flow::make_passive_observer<int>();
        auto fn = [](int x, int y) { return x + y; };
        auto r1 = ctx->make_observable().repeat(11).take(113);
        auto r2 = ctx->make_observable().repeat(22).take(223);
        auto zip = flow::zip_with(fn, std::move(r1), std::move(r2));
        zip.subscribe(o1->as_observer());
        zip.subscribe(o2->as_observer());
        ctx->run();
        REQUIRE_EQ(o1->state, flow::observer_state::subscribed);
        o1->sub.request(64);
        o2->sub.request(64);
        ctx->run();
        CHECK_EQ(o1->state, flow::observer_state::subscribed);
        CHECK_EQ(o1->buf.size(), 64u);
        CHECK_EQ(o2->buf.size(), 64u);
        o1->sub.request(64);
        o2->dispose();
        ctx->run();
        CHECK_EQ(o1->state, flow::observer_state::completed);
        CHECK_EQ(o1->buf.size(), 113u);
        CHECK(std::all_of(o1->buf.begin(), o1->buf.begin(),
                          [](int x) { return x == 33; }));
        CHECK_EQ(o2->buf.size(), 64u);
        CHECK(std::all_of(o2->buf.begin(), o2->buf.begin(),
                          [](int x) { return x == 33; }));
      }
    }
  }
}

END_FIXTURE_SCOPE()
