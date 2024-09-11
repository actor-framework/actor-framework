// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/flow/op/empty.hpp"

#include "caf/test/fixture/flow.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

using namespace caf;

WITH_FIXTURE(test::fixture::flow) {

SCENARIO("an empty observable terminates normally") {
  GIVEN("an empty int32") {
    WHEN("an observer subscribes") {
      THEN("the observer receives on_complete") {
        using snk_t = flow::passive_observer<int32_t>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        make_observable().empty<int32_t>().subscribe(snk->as_observer());
        run_flows();
        check(snk->subscribed());
        snk->request(42);
        run_flows();
        check(snk->completed());
        check(snk->buf.empty());
      }
    }
  }
}

} // WITH_FIXTURE(test::fixture::flow)
