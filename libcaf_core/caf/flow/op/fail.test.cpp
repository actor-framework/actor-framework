// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/fixture/flow.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

using namespace caf;

namespace {

WITH_FIXTURE(test::fixture::flow) {

SCENARIO("the fail operator immediately calls on_error on any subscriber") {
  GIVEN("a fail int32 operator") {
    WHEN("an observer subscribes") {
      THEN("the observer receives on_error") {
        using snk_t = flow::passive_observer<int32_t>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto uut
          = coordinator()->make_observable().fail<int32_t>(sec::runtime_error);
        uut.subscribe(snk->as_observer());
        run_flows();
        check(!snk->sub);
        check_eq(snk->state, flow::observer_state::aborted);
        check(snk->buf.empty());
      }
    }
  }
}

} // WITH_FIXTURE(test::fixture::flow)

} // namespace
