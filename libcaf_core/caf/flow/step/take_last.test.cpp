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
  static auto ls(Ts... xs) {
    return std::vector<int>{xs...};
  }
};

WITH_FIXTURE(fixture) {

TEST("calling take_last(5) on range(1, 100) produces[96, 97, 98, 99, 100]") {
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

TEST("calling take_last(5) on range(1, 5) produces[1, 2, 3, 4, 5]") {
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

TEST("calling take_last(5) on range(1, 3) produces[1, 2, 3]") {
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

TEST("calling take(3) on take_last(5) ignores the last two items") {
  auto result = std::vector<int>{};
  SECTION("blueprint") {
    ctx->make_observable().range(1, 100).take_last(5).take(3).for_each(
      [&](const int& result_observable) {
        result.emplace_back(result_observable);
      });
  }
  SECTION("observable") {
    ctx->make_observable()
      .range(1, 100)
      .as_observable()
      .take_last(5)
      .take(3)
      .for_each([&](const int& result_observable) {
        result.emplace_back(result_observable);
      });
  }
  ctx->run();
  check_eq(result.size(), 3u);
  check_eq(result, ls(96, 97, 98));
}

TEST("take_last operator forwards errors") {
  caf::error result;
  auto outputs = std::vector<int>{};
  SECTION("blueprint") {
    ctx->make_observable()
      .fail<int>(make_error(sec::runtime_error))
      .take_last(5)
      .do_on_error([&result](const error& err) { result = err; })
      .for_each([&](const int& result_observable) {
        outputs.emplace_back(result_observable);
      });
  }
  SECTION("observable") {
    ctx->make_observable()
      .fail<int>(make_error(sec::runtime_error))
      .as_observable()
      .take_last(5)
      .do_on_error([&result](const error& err) { result = err; })
      .for_each([&](const int& result_observable) {
        outputs.emplace_back(result_observable);
      });
  }
  ctx->run();
  check_eq(result, caf::sec::runtime_error);
  check_eq(outputs.size(), 0u);
}

} // WITH_FIXTURE(fixture)

CAF_TEST_MAIN()
