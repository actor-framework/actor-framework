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

SCENARIO("singles emit a value or an error") {
  GIVEN("a single int32") {
    WHEN("an observer subscribes before the values has been set") {
      THEN("the observer receives the value when calling set_value") {
        auto result = std::make_shared<vector<int32_t>>();
        auto err = std::make_shared<error>();
        auto cell = make_counted<op::cell<int32_t>>(coordinator());
        auto uut = single<int32_t>{cell};
        uut.subscribe([result](int32_t x) { result->push_back(x); },
                      [err](const error& x) { *err = x; });
        run_flows();
        check_eq(*result, nil);
        check(!*err);
        cell->set_value(42);
        check_eq(*result, vector{42});
        check(!*err);
      }
    }
    WHEN("an observer subscribes after the values has been set") {
      THEN("the observer receives the value immediately") {
        auto result = std::make_shared<vector<int32_t>>();
        auto err = std::make_shared<error>();
        auto cell = make_counted<op::cell<int32_t>>(coordinator());
        cell->set_value(42);
        auto uut = single<int32_t>{cell};
        uut.subscribe([result](int32_t x) { result->push_back(x); },
                      [err](const error& x) { *err = x; });
        run_flows();
        check_eq(*result, vector{42});
        check(!*err);
      }
    }
    WHEN("the cell emits an error") {
      THEN("the observer receives the error") {
        auto result = std::make_shared<vector<int32_t>>();
        auto err = std::make_shared<error>();
        auto cell = make_counted<op::cell<int32_t>>(coordinator());
        auto uut = single<int32_t>{cell};
        uut.subscribe([result](int32_t x) { result->push_back(x); },
                      [err](const error& x) { *err = x; });
        cell->set_error(sec::runtime_error);
        run_flows();
        check_eq(*result, nil);
        check_eq(*err, sec::runtime_error);
      }
    }
  }
}

} // WITH_FIXTURE(test::fixture::flow)
