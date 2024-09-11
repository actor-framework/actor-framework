// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/flow/multicaster.hpp"

#include "caf/test/caf_test_main.hpp"
#include "caf/test/fixture/flow.hpp"
#include "caf/test/nil.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

using caf::test::nil;
using std::vector;

using namespace caf;
using namespace caf::flow;

WITH_FIXTURE(test::fixture::flow) {

SCENARIO("a multicaster pushes items to all subscribers") {
  GIVEN("a multicaster with two subscribers") {
    auto uut = multicaster<int>{coordinator()};
    auto snk1 = coordinator()->add_child(
      std::in_place_type<flow::passive_observer<int>>);
    auto snk2 = coordinator()->add_child(
      std::in_place_type<flow::passive_observer<int>>);
    uut.subscribe(snk1->as_observer());
    uut.subscribe(snk2->as_observer());
    check(uut.impl().has_observers());
    check_eq(uut.impl().observer_count(), 2u);
    WHEN("pushing items") {
      THEN("all observers see all items") {
        check_eq(uut.impl().min_demand(), 0u);
        check_eq(uut.impl().max_demand(), 0u);
        check_eq(uut.impl().min_buffered(), 0u);
        check_eq(uut.impl().max_buffered(), 0u);
        // Push 3 items with no demand.
        check_eq(uut.push({1, 2, 3}), 0u);
        run_flows();
        check_eq(uut.impl().min_demand(), 0u);
        check_eq(uut.impl().max_demand(), 0u);
        check_eq(uut.impl().min_buffered(), 3u);
        check_eq(uut.impl().max_buffered(), 3u);
        check_eq(snk1->buf, nil);
        check_eq(snk2->buf, nil);
        // Pull out one item with snk1.
        snk1->sub.request(1);
        run_flows();
        check_eq(uut.impl().min_demand(), 0u);
        check_eq(uut.impl().max_demand(), 0u);
        check_eq(uut.impl().min_buffered(), 2u);
        check_eq(uut.impl().max_buffered(), 3u);
        check_eq(snk1->buf, vector{1});
        check_eq(snk2->buf, nil);
        // Pull out all item with snk1 plus 2 extra demand.
        snk1->sub.request(4);
        run_flows();
        check_eq(uut.impl().min_demand(), 0u);
        check_eq(uut.impl().max_demand(), 2u);
        check_eq(uut.impl().min_buffered(), 0u);
        check_eq(uut.impl().max_buffered(), 3u);
        check_eq(snk1->buf, vector{1, 2, 3});
        check_eq(snk2->buf, nil);
        // Pull out all items with snk2 plus 4 extra demand.
        snk2->sub.request(7);
        run_flows();
        check_eq(uut.impl().min_demand(), 2u);
        check_eq(uut.impl().max_demand(), 4u);
        check_eq(uut.impl().min_buffered(), 0u);
        check_eq(uut.impl().max_buffered(), 0u);
        check_eq(snk1->buf, vector{1, 2, 3});
        check_eq(snk2->buf, vector{1, 2, 3});
        // Push 3 more items, expect 2 to be dispatched immediately.
        check_eq(uut.push({4, 5, 6}), 2u);
        check_eq(uut.impl().min_demand(), 0u);
        check_eq(uut.impl().max_demand(), 1u);
        check_eq(uut.impl().min_buffered(), 0u);
        check_eq(uut.impl().max_buffered(), 1u);
        check_eq(snk1->buf, vector{1, 2, 3, 4, 5});
        check_eq(snk2->buf, vector{1, 2, 3, 4, 5, 6});
        // Pull out the remaining element with snk1 plus 9 extra demand.
        snk1->sub.request(10);
        run_flows();
        check_eq(uut.impl().min_demand(), 1u);
        check_eq(uut.impl().max_demand(), 9u);
        check_eq(uut.impl().min_buffered(), 0u);
        check_eq(uut.impl().max_buffered(), 0u);
        check_eq(snk1->buf, vector{1, 2, 3, 4, 5, 6});
        check_eq(snk2->buf, vector{1, 2, 3, 4, 5, 6});
        // Close: must call on_complete immediately since buffers are empty.
        uut.close();
        check_eq(snk1->state, flow::observer_state::completed);
        check_eq(snk2->state, flow::observer_state::completed);
      }
    }
  }
}

SCENARIO("a multicaster discards items that arrive before a subscriber") {
  WHEN("pushing items") {
    THEN("observers see only items that were pushed after subscribing") {
      using snk_t = auto_observer<int>;
      auto uut = multicaster<int>{coordinator()};
      uut.push({1, 2, 3});
      auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
      uut.subscribe(snk->as_observer());
      run_flows();
      uut.push({4, 5, 6});
      run_flows();
      uut.close();
      check_eq(snk->buf, vector{4, 5, 6});
      check_eq(snk->state, observer_state::completed);
    }
  }
}

} // WITH_FIXTURE(test::fixture::flow)
