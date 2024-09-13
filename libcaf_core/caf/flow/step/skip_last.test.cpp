// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/flow/step/skip_last.hpp"

#include "caf/test/fixture/flow.hpp"
#include "caf/test/test.hpp"

#include "caf/flow/scoped_coordinator.hpp"
#include "caf/scheduled_actor/flow.hpp"

#include <vector>

using namespace caf;

WITH_FIXTURE(test::fixture::flow) {

TEST("calling skip_last(5) on range(1, 10) produces [1, 2, 3, 4, 5]") {
  SECTION("blueprint") {
    check_eq(collect(range(1, 10).skip_last(5)), std::vector{1, 2, 3, 4, 5});
  }
  SECTION("observable") {
    check_eq(collect(range(1, 10).as_observable().skip_last(5)),
             std::vector{1, 2, 3, 4, 5});
  }
}

TEST("calling skip_last(5) on range(1, 5) produces []") {
  SECTION("blueprint") {
    check_eq(collect(range(1, 5).skip_last(5)), std::vector<int>{});
  }
  SECTION("observable") {
    check_eq(collect(range(1, 5).as_observable().skip_last(5)),
             std::vector<int>{});
  }
}

TEST("calling skip_last(5) on range(1, 3) produces []") {
  SECTION("blueprint") {
    check_eq(collect(range(1, 3).skip_last(5)), std::vector<int>{});
  }
  SECTION("observable") {
    check_eq(collect(range(1, 3).as_observable().skip_last(5)),
             std::vector<int>{});
  }
}

TEST("calling take(3) on skip_last(5) ignores the last two items") {
  SECTION("blueprint") {
    check_eq(collect(range(1, 10).skip_last(5).take(3)), std::vector{1, 2, 3});
  }
  SECTION("observable") {
    check_eq(collect(range(1, 10).as_observable().skip_last(5).take(3)),
             std::vector{1, 2, 3});
  }
}

TEST("skip_last operator forwards errors") {
  SECTION("blueprint") {
    check_eq(collect(obs_error<int>().skip_last(5)),
             make_error(sec::runtime_error));
  }
  SECTION("observable") {
    check_eq(collect(obs_error<int>().as_observable().skip_last(5)),
             make_error(sec::runtime_error));
  }
}

} // WITH_FIXTURE(test::fixture::flow)
