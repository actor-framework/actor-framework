// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/flow/op/never.hpp"

#include "caf/test/caf_test_main.hpp"
#include "caf/test/fixture/flow.hpp"
#include "caf/test/scenario.hpp"

using namespace caf;

namespace {

WITH_FIXTURE(test::fixture::flow) {

SCENARIO("the never operator never invokes callbacks except when disposed") {
  GIVEN("a never operator") {
    WHEN("an observer subscribes and disposing the subscription") {
      THEN("the observer receives on_complete") {
        using snk_t = auto_observer<int32_t>;
        auto uut = coordinator()->make_observable().never<int32_t>();
        auto snk1 = coordinator()->add_child(std::in_place_type<snk_t>);
        auto snk2 = coordinator()->add_child(std::in_place_type<snk_t>);
        auto sub1 = uut.subscribe(snk1->as_observer());
        run_flows();
        check(snk1->buf.empty());
        check_eq(snk1->state, flow::observer_state::subscribed);
        sub1.ptr()->dispose();
        run_flows();
        check(sub1.ptr()->disposed());
        check(snk1->aborted());
        auto sub2 = uut.subscribe(snk2->as_observer());
        run_flows();
        check_eq(snk2->state, flow::observer_state::subscribed);
        sub2.dispose();
        run_flows();
      }
    }
  }
}

} // WITH_FIXTURE(test::fixture::flow)

} // namespace
