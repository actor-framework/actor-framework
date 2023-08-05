// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE flow.op.mcast

#include "caf/flow/op/mcast.hpp"

#include "caf/flow/observable.hpp"
#include "caf/flow/scoped_coordinator.hpp"

#include "core-test.hpp"

using namespace caf;

namespace {

using int_mcast = flow::op::mcast<int>;

using int_mcast_ptr = intrusive_ptr<int_mcast>;

struct fixture : test_coordinator_fixture<> {
  flow::scoped_coordinator_ptr ctx = flow::make_scoped_coordinator();

  int_mcast_ptr make_mcast() {
    return make_counted<int_mcast>(ctx.get());
  }

  auto lift(int_mcast_ptr mcast) {
    return flow::observable<int>{mcast};
  }
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

SCENARIO("closed mcast operators appear empty") {
  GIVEN("a closed mcast operator") {
    WHEN("subscribing to it") {
      THEN("the observer receives an on_complete event") {
        auto uut = make_mcast();
        uut->close();
        auto snk = flow::make_auto_observer<int>();
        lift(uut).subscribe(snk->as_observer());
        ctx->run();
        CHECK(snk->completed());
      }
    }
  }
}

SCENARIO("aborted mcast operators fail when subscribed") {
  GIVEN("an aborted mcast operator") {
    WHEN("subscribing to it") {
      THEN("the observer receives an on_error event") {
        auto uut = make_mcast();
        uut->abort(sec::runtime_error);
        auto snk = flow::make_auto_observer<int>();
        lift(uut).subscribe(snk->as_observer());
        ctx->run();
        CHECK(snk->aborted());
      }
    }
  }
}

SCENARIO("mcast operators buffer items that they cannot ship immediately") {
  GIVEN("an mcast operator with three observers") {
    WHEN("pushing more data than the observers have requested") {
      THEN("items are buffered individually") {
        MESSAGE("subscribe three observers to a fresh mcast operator");
        auto uut = make_mcast();
        CHECK(!uut->has_observers());
        CHECK_EQ(uut->observer_count(), 0u);
        CHECK_EQ(uut->max_demand(), 0u);
        CHECK_EQ(uut->min_demand(), 0u);
        CHECK_EQ(uut->max_buffered(), 0u);
        CHECK_EQ(uut->min_buffered(), 0u);
        auto o1 = flow::make_passive_observer<int>();
        auto o2 = flow::make_passive_observer<int>();
        auto o3 = flow::make_passive_observer<int>();
        CHECK_EQ(uut->observer_count(), 0u);
        auto sub1 = uut->subscribe(o1->as_observer());
        CHECK_EQ(uut->observer_count(), 1u);
        auto sub2 = uut->subscribe(o2->as_observer());
        CHECK_EQ(uut->observer_count(), 2u);
        auto sub3 = uut->subscribe(o3->as_observer());
        CHECK(uut->has_observers());
        CHECK_EQ(uut->observer_count(), 3u);
        CHECK_EQ(uut->max_demand(), 0u);
        CHECK_EQ(uut->min_demand(), 0u);
        CHECK_EQ(uut->max_buffered(), 0u);
        CHECK_EQ(uut->min_buffered(), 0u);
        MESSAGE("trigger request for items");
        o1->request(3);
        o2->request(5);
        o3->request(7);
        ctx->run();
        CHECK_EQ(uut->max_demand(), 7u);
        CHECK_EQ(uut->min_demand(), 3u);
        CHECK_EQ(uut->max_buffered(), 0u);
        CHECK_EQ(uut->min_buffered(), 0u);
        MESSAGE("push more items than we have demand for");
        for (auto i = 0; i < 8; ++i)
          uut->push_all(i);
        CHECK_EQ(uut->max_demand(), 0u);
        CHECK_EQ(uut->min_demand(), 0u);
        CHECK_EQ(uut->max_buffered(), 5u);
        CHECK_EQ(uut->min_buffered(), 1u);
        MESSAGE("drop the subscriber with the largest buffer");
        sub1.dispose();
        ctx->run();
        CHECK_EQ(uut->max_demand(), 0u);
        CHECK_EQ(uut->min_demand(), 0u);
        CHECK_EQ(uut->max_buffered(), 3u);
        CHECK_EQ(uut->min_buffered(), 1u);
        sub2.dispose();
        sub3.dispose();
      }
    }
  }
}

END_FIXTURE_SCOPE()
