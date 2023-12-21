// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/flow/op/mcast.hpp"

#include "caf/test/fixture/flow.hpp"
#include "caf/test/nil.hpp"
#include "caf/test/scenario.hpp"

#include "caf/flow/observable.hpp"
#include "caf/flow/scoped_coordinator.hpp"
#include "caf/log/test.hpp"

using namespace caf;

namespace {

using int_mcast = flow::op::mcast<int>;

using int_mcast_ptr = intrusive_ptr<int_mcast>;

struct fixture : test::fixture::flow {
  int_mcast_ptr make_mcast() {
    return coordinator()->add_child(std::in_place_type<int_mcast>);
  }

  auto lift(int_mcast_ptr mcast) {
    return caf::flow::observable<int>{mcast};
  }
};

WITH_FIXTURE(fixture) {

SCENARIO("closed mcast operators appear empty") {
  GIVEN("a closed mcast operator") {
    WHEN("subscribing to it") {
      THEN("the observer receives an on_complete event") {
        auto uut = make_mcast();
        uut->close();
        auto snk = make_auto_observer<int>();
        lift(uut).subscribe(snk->as_observer());
        run_flows();
        check(snk->completed());
      }
    }
  }
}

SCENARIO("aborted mcast operators fail when subscribed to") {
  GIVEN("an aborted mcast operator") {
    WHEN("subscribing to it") {
      THEN("the observer receives an on_error event") {
        auto uut = make_mcast();
        uut->abort(sec::runtime_error);
        auto snk = flow::make_auto_observer<int>();
        lift(uut).subscribe(snk->as_observer());
        run_flows();
        check(snk->aborted());
      }
    }
  }
}

SCENARIO("mcast operators buffer items that they cannot ship immediately") {
  GIVEN("an mcast operator with three observers") {
    WHEN("pushing more data than the observers have requested") {
      THEN("items are buffered individually") {
        log::test::debug("subscribe three observers to a fresh mcast operator");
        auto uut = make_mcast();
        check(!uut->has_observers());
        check_eq(uut->observer_count(), 0u);
        check_eq(uut->max_demand(), 0u);
        check_eq(uut->min_demand(), 0u);
        check_eq(uut->max_buffered(), 0u);
        check_eq(uut->min_buffered(), 0u);
        auto o1 = flow::make_passive_observer<int>();
        auto o2 = flow::make_passive_observer<int>();
        auto o3 = flow::make_passive_observer<int>();
        check_eq(uut->observer_count(), 0u);
        auto sub1 = uut->subscribe(o1->as_observer());
        check_eq(uut->observer_count(), 1u);
        auto sub2 = uut->subscribe(o2->as_observer());
        check_eq(uut->observer_count(), 2u);
        auto sub3 = uut->subscribe(o3->as_observer());
        check(uut->has_observers());
        check_eq(uut->observer_count(), 3u);
        check_eq(uut->max_demand(), 0u);
        check_eq(uut->min_demand(), 0u);
        check_eq(uut->max_buffered(), 0u);
        check_eq(uut->min_buffered(), 0u);
        log::test::debug("trigger request for items");
        o1->request(3);
        o2->request(5);
        o3->request(7);
        run_flows();
        check_eq(uut->max_demand(), 7u);
        check_eq(uut->min_demand(), 3u);
        check_eq(uut->max_buffered(), 0u);
        check_eq(uut->min_buffered(), 0u);
        log::test::debug("push more items than we have demand for");
        for (auto i = 0; i < 8; ++i)
          uut->push_all(i);
        check_eq(uut->max_demand(), 0u);
        check_eq(uut->min_demand(), 0u);
        check_eq(uut->max_buffered(), 5u);
        check_eq(uut->min_buffered(), 1u);
        log::test::debug("drop the subscriber with the largest buffer");
        sub1.dispose();
        run_flows();
        check_eq(uut->max_demand(), 0u);
        check_eq(uut->min_demand(), 0u);
        check_eq(uut->max_buffered(), 3u);
        check_eq(uut->min_buffered(), 1u);
        sub2.dispose();
        sub3.dispose();
        run_flows();
      }
    }
  }
}

} // WITH_FIXTURE(fixture)

} // namespace
