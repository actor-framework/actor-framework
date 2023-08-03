// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/flow/step/take_last.hpp"

#include "caf/test/caf_test_main.hpp"
#include "caf/test/test.hpp"

#include "caf/flow/scoped_coordinator.hpp"
#include "caf/scheduled_actor/flow.hpp"

#include <vector>

using namespace caf;

using caf::flow::make_observer;

struct fixture {
  flow::scoped_coordinator_ptr ctx = flow::make_scoped_coordinator();

  template <class... Ts>
  std::vector<int> ls(Ts... xs) {
    return std::vector<int>{xs...};
  }
};

WITH_FIXTURE(fixture) {

TEST("take the last 5 numbers in a series of 100 numbers") {
  auto result = std::vector<int>{};
  SECTION("blueprint") {
    ctx->make_observable().range(1, 100).take_last(5).for_each(
      [&](const int& result_observable) {
        result.emplace_back(result_observable);
      });
  }
  SECTION("observable") {
    ctx->make_observable().range(1, 100).as_observable().take_last(5).for_each(
      [&](const int& result_observable) {
        result.emplace_back(result_observable);
      });
  }
  ctx->run();
  check_eq(result.size(), 5u);
  check_eq(result, ls(96, 97, 98, 99, 100));
}

TEST("take the last 5 numbers in a series of 5 numbers") {
  auto result = std::vector<int>{};
  SECTION("blueprint") {
    ctx->make_observable().range(1, 5).take_last(5).for_each(
      [&](const int& result_observable) {
        result.emplace_back(result_observable);
      });
  }
  SECTION("observable") {
    ctx->make_observable().range(1, 5).as_observable().take_last(5).for_each(
      [&](const int& result_observable) {
        result.emplace_back(result_observable);
      });
  }
  ctx->run();
  check_eq(result.size(), 5u);
  check_eq(result, ls(1, 2, 3, 4, 5));
}

TEST("take the last 5 numbers in a series of 3 numbers") {
  auto result = std::vector<int>{};
  SECTION("blueprint") {
    ctx->make_observable().range(1, 3).take_last(5).for_each(
      [&](const int& result_observable) {
        result.emplace_back(result_observable);
      });
  }
  SECTION("observable") {
    ctx->make_observable().range(1, 3).as_observable().take_last(5).for_each(
      [&](const int& result_observable) {
        result.emplace_back(result_observable);
      });
  }
  ctx->run();
  check_eq(result.size(), 3u);
  check_eq(result, ls(1, 2, 3));
}
}

CAF_TEST_MAIN()
