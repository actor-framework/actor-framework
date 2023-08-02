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

TEST("take the last 5 digits in a series of 100 digits") {
  SECTION("blueprint") {
    auto result = std::vector<std::vector<int>>{};
    ctx->make_observable().range(1, 100).take_last(5).to_vector().for_each(
      [&](const caf::cow_vector<int>& result_cow) {
        result.emplace_back(result_cow.std());
      });
    ctx->run();
    check_eq(result.size(), 1u);
    check_eq(result.at(0), ls(96, 97, 98, 99, 100));
  }
  SECTION("observable") {
    auto result = std::vector<std::vector<int>>{};
    ctx->make_observable()
      .range(1, 100)
      .as_observable()
      .take_last(5)
      .to_vector()
      .for_each([&](const caf::cow_vector<int>& result_cow) {
        result.emplace_back(result_cow.std());
      });
    ctx->run();
    check_eq(result.size(), 1u);
    check_eq(result.at(0), ls(96, 97, 98, 99, 100));
  }
}

TEST("take the last 5 digits in a series of 5 digits") {
  SECTION("blueprint") {
    auto result = std::vector<std::vector<int>>{};
    ctx->make_observable().range(1, 5).take_last(5).to_vector().for_each(
      [&](const caf::cow_vector<int>& result_cow) {
        result.emplace_back(result_cow.std());
      });
    ctx->run();
    check_eq(result.size(), 1u);
    check_eq(result.at(0), ls(1, 2, 3, 4, 5));
  }
  SECTION("observable") {
    auto result = std::vector<std::vector<int>>{};
    ctx->make_observable()
      .range(1, 5)
      .as_observable()
      .take_last(5)
      .to_vector()
      .for_each([&](const caf::cow_vector<int>& result_cow) {
        result.emplace_back(result_cow.std());
      });
    ctx->run();
    check_eq(result.size(), 1u);
    check_eq(result.at(0), ls(1, 2, 3, 4, 5));
  }
}

TEST("take the last 5 digits in a series of 3 digits") {
  SECTION("blueprint") {
    auto result = std::vector<std::vector<int>>{};
    ctx->make_observable().range(1, 5).take_last(5).to_vector().for_each(
      [&](const caf::cow_vector<int>& result_cow) {
        result.emplace_back(result_cow.std());
      });
    ctx->run();
    check_eq(result.size(), 1u);
    check_eq(result.at(0), ls(1, 2, 3, 4, 5));
  }
  SECTION("observable") {
    auto result = std::vector<std::vector<int>>{};
    ctx->make_observable()
      .range(1, 2)
      .as_observable()
      .take_last(5)
      .to_vector()
      .for_each([&](const caf::cow_vector<int>& result_cow) {
        result.emplace_back(result_cow.std());
      });
    ctx->run();
    check_eq(result.size(), 1u);
    check_eq(result.at(0), ls(1, 2));
  }
}

}

CAF_TEST_MAIN()
