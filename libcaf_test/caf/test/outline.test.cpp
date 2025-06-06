// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/test/outline.hpp"

#include "caf/test/approx.hpp"

#include "caf/log/test.hpp"

#include <numeric>

OUTLINE("eating cucumbers") {
  GIVEN("there are <start> cucumbers") {
    auto start = block_parameters<int>();
    auto cucumbers = start;
    caf::log::test::debug("cucumbers: {}", cucumbers);
    WHEN("I eat <eat> cucumbers") {
      auto eat = block_parameters<int>();
      cucumbers -= eat;
      caf::log::test::debug("cucumbers: {}", cucumbers);
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

OUTLINE("adding two numbers") {
  GIVEN("the numbers <x> and <y>") {
    auto [x, y] = block_parameters<double, double>();
    WHEN("adding both numbers") {
      auto result = x + y;
      THEN("the result should be <sum>") {
        auto sum = block_parameters<double>();
        check_eq(result, caf::test::approx{sum});
      }
    }
  }
  EXAMPLES = R"(
    |   x |   y | sum |
    |   1 |   2 |   3 |
    | 2.5 | 3.5 |   6 |
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

OUTLINE("an outline with multiple when blocks") {
  GIVEN("the numbers <x> and <y>") {
    auto [x, y] = block_parameters<double, double>();
    WHEN("adding both numbers") {
      auto result = x + y;
      THEN("the result should be <sum>") {
        auto sum = block_parameters<double>();
        check_eq(result, caf::test::approx{sum});
      }
    }
    WHEN("subtracting both numbers") {
      auto result = x - y;
      THEN("the result should be <diff>") {
        auto diff = block_parameters<double>();
        check_eq(result, caf::test::approx{diff});
      }
    }
  }
  EXAMPLES = R"(
    |   x |   y | sum | diff |
    |   1 |   2 |   3 |  -1 |
    | 2.5 | 3.5 |   6 |  -1 |
  )";
}
