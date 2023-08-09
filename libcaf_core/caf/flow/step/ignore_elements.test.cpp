// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/flow/step/ignore_elements.hpp"

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

TEST("calling ignore_elements on range(1, 10) produces []") {
  auto result = std::vector<int>{};
  SECTION("blueprint") {
    ctx->make_observable().range(1, 10).ignore_elements().for_each(
      [&](const int& result_observable) {
        result.emplace_back(result_observable);
      });
  }
  SECTION("observable") {
    ctx->make_observable()
      .range(1, 10)
      .as_observable()
      .ignore_elements()
      .for_each([&](const int& result_observable) {
        result.emplace_back(result_observable);
      });
  }
  ctx->run();
  check_eq(result.size(), 0u);
}

TEST("ignore_elements operator forwards errors") {
  caf::error result;
  auto outputs = std::vector<int>{};
  SECTION("blueprint") {
    ctx->make_observable()
      .fail<int>(make_error(sec::runtime_error))
      .ignore_elements()
      .do_on_error([&result](const error& err) { result = err; })
      .for_each([&](const int& result_observable) {
        outputs.emplace_back(result_observable);
      });
  }
  SECTION("observable") {
    ctx->make_observable()
      .fail<int>(make_error(sec::runtime_error))
      .as_observable()
      .ignore_elements()
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
