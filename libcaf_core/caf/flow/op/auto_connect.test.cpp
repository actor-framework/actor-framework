// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/flow/op/auto_connect.hpp"

#include "caf/test/fixture/flow.hpp"
#include "caf/test/scenario.hpp"

#include "caf/flow/op/publish.hpp"
#include "caf/flow/op/ucast.hpp"

using namespace caf;

WITH_FIXTURE(caf::test::fixture::flow) {

SCENARIO("auto_connect operators call connect when reaching the threshold") {
  GIVEN("a connectable with an input observable") {
    auto src = make_counted<caf::flow::op::ucast<int>>(coordinator());
    for (int val = 0; val < 5; ++val) {
      src->push(val);
    }
    auto pub = make_counted<caf::flow::op::publish<int>>(coordinator(), src);
    check_eq(pub->observer_count(), 0u);
    WHEN("creating an auto_connect operator with a threshold of 2") {
      auto uut = make_counted<caf::flow::op::auto_connect<int>>(coordinator(),
                                                                2, pub);
      check_eq(uut->pending_subscriptions_count(), 0u);
      check_eq(pub->observer_count(), 0u);
      auto snk1 = make_auto_observer<int>();
      auto snk2 = make_auto_observer<int>();
      THEN("the operator connects after the second subscription") {
        uut->subscribe(snk1->as_observer());
        check_eq(uut->pending_subscriptions_count(), 1u);
        check_eq(pub->observer_count(), 0u);
        uut->subscribe(snk2->as_observer());
        check_eq(uut->pending_subscriptions_count(), 0u);
        check_eq(pub->observer_count(), 2u);
        run_flows();
        check_eq(snk1->buf, std::vector{0, 1, 2, 3, 4});
        check_eq(snk2->buf, std::vector{0, 1, 2, 3, 4});
      }
      AND_THEN("a third subscription no longer sees the initial items") {
        auto snk3 = make_auto_observer<int>();
        uut->subscribe(snk3->as_observer());
        run_flows();
        check_eq(snk1->buf, std::vector{0, 1, 2, 3, 4});
        check_eq(snk2->buf, std::vector{0, 1, 2, 3, 4});
        check(snk3->buf.empty());
        src->push(5);
        run_flows();
        check_eq(snk1->buf, std::vector{0, 1, 2, 3, 4, 5});
        check_eq(snk2->buf, std::vector{0, 1, 2, 3, 4, 5});
        check_eq(snk3->buf, std::vector{5});
      }
    }
  }
}

SCENARIO("auto_connect operators stay connected even without subscribers") {
  GIVEN("a connectable with an input observable") {
    auto src = make_counted<caf::flow::op::ucast<int>>(coordinator());
    auto pub = make_counted<caf::flow::op::publish<int>>(coordinator(), src);
    auto uut = make_counted<caf::flow::op::auto_connect<int>>(coordinator(), 2,
                                                              pub);
    WHEN("two subscribers subscribe and then cancel their subscriptions") {
      auto snk1 = make_auto_observer<int>();
      auto snk2 = make_auto_observer<int>();
      uut->subscribe(snk1->as_observer());
      uut->subscribe(snk2->as_observer());
      check(uut->connected());
      check(pub->connected());
      src->push(0);
      run_flows();
      snk1->sub.cancel();
      snk2->sub.cancel();
      THEN("the operator stays connected and new subscribers see new items") {
        check(uut->connected());
        check(pub->connected());
        src->push(1); // lost, because no subscribers exist anymore
        auto snk3 = make_auto_observer<int>();
        uut->subscribe(snk3->as_observer());
        src->push(2); // arrives at snk3
        run_flows();
        check_eq(snk1->buf, std::vector{0});
        check_eq(snk2->buf, std::vector{0});
        check_eq(snk3->buf, std::vector{2});
      }
    }
  }
}

} // WITH_FIXTURE(caf::test::fixture::flow)
