// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/flow/op/ref_count.hpp"

#include "caf/test/fixture/flow.hpp"
#include "caf/test/scenario.hpp"

#include "caf/flow/op/mcast.hpp"
#include "caf/flow/op/publish.hpp"

using namespace caf;

WITH_FIXTURE(caf::test::fixture::flow) {

SCENARIO("a ref_count operator disconnects when no subscribers exist") {
  GIVEN("a connectable with an input observable") {
    auto src = make_counted<caf::flow::op::mcast<int>>(coordinator());
    auto pub = make_counted<caf::flow::op::publish<int>>(coordinator(), src);
    auto uut = make_counted<caf::flow::op::ref_count<int>>(coordinator(), 2,
                                                           pub);
    WHEN("two subscribers subscribe and then cancel their subscriptions") {
      auto snk1 = make_auto_observer<int>();
      auto snk2 = make_auto_observer<int>();
      uut->subscribe(snk1->as_observer());
      uut->subscribe(snk2->as_observer());
      check(uut->connected());
      check(pub->connected());
      src->push_all(0);
      run_flows();
      snk1->sub.cancel();
      snk2->sub.cancel();
      run_flows();
      THEN("the operator disconnects and re-connects when subscribed again") {
        check(!uut->connected());
        check(!pub->connected());
        src->push_all(1); // lost, because no subscribers exist anymore
        auto snk3 = make_auto_observer<int>();
        uut->subscribe(snk3->as_observer());
        run_flows();
        check(uut->connected());
        check(pub->connected());
        src->push_all(2); // arrives at snk3
        run_flows();
        check_eq(snk1->buf, std::vector{0});
        check_eq(snk2->buf, std::vector{0});
        check_eq(snk3->buf, std::vector{2});
      }
    }
  }
}

} // WITH_FIXTURE(caf::test::fixture::flow)
