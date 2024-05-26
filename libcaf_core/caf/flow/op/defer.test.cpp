// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/flow/op/defer.hpp"

#include "caf/test/caf_test_main.hpp"
#include "caf/test/fixture/flow.hpp"
#include "caf/test/scenario.hpp"

using namespace caf;

namespace {

WITH_FIXTURE(test::fixture::flow) {

SCENARIO("the defer operator produces a fresh observable for each observer") {
  GIVEN("a deferred observable") {
    WHEN("two observers subscribes") {
      THEN("each observer subscribes to a fresh observable") {
        using ivec = std::vector<int>;
        using snk_t = flow::passive_observer<int>;
        size_t factory_calls = 0;
        auto factory = [this, &factory_calls] {
          ++factory_calls;
          return coordinator()->make_observable().iota(1).take(5);
        };
        auto uut = coordinator()->make_observable().defer(factory);
        auto snk1 = coordinator()->add_child(std::in_place_type<snk_t>);
        auto snk2 = coordinator()->add_child(std::in_place_type<snk_t>);
        uut.subscribe(snk1->as_observer());
        check_eq(factory_calls, 1u);
        uut.subscribe(snk2->as_observer());
        check_eq(factory_calls, 2u);
        check(snk1->request(27));
        check(snk2->request(3));
        run_flows();
        check_eq(snk1->state, flow::observer_state::completed);
        check_eq(snk1->buf, ivec({1, 2, 3, 4, 5}));
        check_eq(snk2->state, flow::observer_state::subscribed);
        check_eq(snk2->buf, ivec({1, 2, 3}));
        check(snk2->request(2));
        run_flows();
        check_eq(snk2->state, flow::observer_state::completed);
        check_eq(snk2->buf, ivec({1, 2, 3, 4, 5}));
      }
    }
  }
}

} // WITH_FIXTURE(test::fixture::flow)

} // namespace
