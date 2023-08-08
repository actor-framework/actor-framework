// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/fixture/flow.hpp"

#include "caf/test/caf_test_main.hpp"
#include "caf/test/test.hpp"

#include "caf/flow/observable.hpp"

using namespace caf;

WITH_FIXTURE(test::fixture::flow) {

TEST("range() creates a blueprint for a range of numbers") {
  auto values = std::make_shared<std::vector<int>>();
  range(1, 3).for_each([values](int value) { //
    values->emplace_back(value);
  });
  run_flows();
  check_eq(*values, std::vector{1, 2, 3});
}

TEST("collect() eagerly evaluates an observable and returns a vector") {
  check_eq(collect(range(1, 0)), std::vector<int>{});
  check_eq(collect(range(1, 1)), std::vector{1});
  check_eq(collect(range(1, 2)), std::vector{1, 2});
  check_eq(collect(range(1, 3)), std::vector{1, 2, 3});
}

} // WITH_FIXTURE(test::fixture::flow)

CAF_TEST_MAIN()
