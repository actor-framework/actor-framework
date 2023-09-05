// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/outline.hpp"

#include "caf/test/caf_test_main.hpp"

#include <numeric>

OUTLINE("eating cucumbers") {
  GIVEN("there are <start> cucumbers") {
    auto start = block_parameters<int>();
    auto cucumbers = start;
    print_debug("cucumbers: {}", cucumbers);
    WHEN("I eat <eat> cucumbers") {
      auto eat = block_parameters<int>();
      cucumbers -= eat;
      print_debug("cucumbers: {}", cucumbers);
      THEN("I should have <left> cucumbers") {
        auto left = block_parameters<int>();
        check_eq(cucumbers, left);
      }
    }
  }
  EXAMPLES = R"(
    | start | eat | left |
    |    12 |   5 |    7 |
    |    20 |   5 |   15 |
  )";
}

OUTLINE("counting numbers") {
  GIVEN("the list <values>") {
    auto values = block_parameters<std::vector<int>>();
    WHEN("accumulating all values") {
      auto result = std::accumulate(values.begin(), values.end(), 0);
      THEN("the result should be <sum>") {
        auto sum = block_parameters<int>();
        check_eq(result, sum);
      }
    }
  }
  // Note: unused variables are ignored.
  EXAMPLES = R"(
    |    values | sum | unused |
    |        [] |   0 |      1 |
    |       [1] |   1 |    foo |
    |    [1, 2] |   3 |    bar |
    | [1, 2, 3] |   6 |   okay |
  )";
}

CAF_TEST_MAIN()
