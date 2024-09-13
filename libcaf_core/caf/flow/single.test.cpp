// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/flow/single.hpp"

#include "caf/test/caf_test_main.hpp"
#include "caf/test/fixture/flow.hpp"
#include "caf/test/nil.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/flow/op/cell.hpp"

using caf::test::nil;
using std::vector;

using namespace caf;
using namespace caf::flow;

WITH_FIXTURE(test::fixture::flow) {

SCENARIO("singles emit at most one value") {
  GIVEN("a single int32") {
    WHEN("an observer subscribes before the values has been set") {
      THEN("the observer receives the value when calling set_value") {
        auto outputs = vector<int32_t>{};
        auto cell = make_counted<op::cell<int32_t>>(coordinator());
        auto single_int = single<int32_t>{cell};
        single_int //
          .as_observable()
          .for_each([&outputs](int32_t x) { outputs.emplace_back(x); });
        run_flows();
        check_eq(outputs, nil);
        cell->set_value(42);
        check_eq(outputs, vector{42});
      }
    }
    WHEN("an observer subscribes after the values has been set") {
      THEN("the observer receives the value immediately") {
        auto cell = make_counted<op::cell<int32_t>>(coordinator());
        auto single_int = single<int32_t>{cell};
        cell->set_value(42);
        check_eq(collect(single_int.as_observable()), vector{42});
      }
    }
  }
}

} // WITH_FIXTURE(test::fixture::flow)
