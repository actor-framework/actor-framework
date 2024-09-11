// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/flow/step/take_last.hpp"

#include "caf/test/fixture/flow.hpp"
#include "caf/test/test.hpp"

#include "caf/flow/scoped_coordinator.hpp"
#include "caf/scheduled_actor/flow.hpp"

#include <vector>

using namespace caf;

WITH_FIXTURE(test::fixture::flow) {

TEST("calling take_last(5) on range(1, 100) produces[96, 97, 98, 99, 100]") {
  SECTION("blueprint") {
    check_eq(collect(range(1, 100).take_last(5)),
             std::vector{96, 97, 98, 99, 100});
  }
  SECTION("observable") {
    check_eq(collect(range(1, 100).as_observable().take_last(5)),
             std::vector{96, 97, 98, 99, 100});
  }
}

TEST("calling take_last(5) on range(1, 5) produces[1, 2, 3, 4, 5]") {
  SECTION("blueprint") {
    check_eq(collect(range(1, 5).take_last(5)), std::vector{1, 2, 3, 4, 5});
  }
  SECTION("observable") {
    check_eq(collect(range(1, 5).as_observable().take_last(5)),
             std::vector{1, 2, 3, 4, 5});
  }
}

TEST("calling take_last(5) on range(1, 3) produces[1, 2, 3]") {
  SECTION("blueprint") {
    check_eq(collect(range(1, 3).take_last(5)), std::vector{1, 2, 3});
  }
  SECTION("observable") {
    check_eq(collect(range(1, 3).as_observable().take_last(5)),
             std::vector{1, 2, 3});
  }
}

TEST("calling take(3) on take_last(5) ignores the last two items") {
  SECTION("blueprint") {
    check_eq(collect(range(1, 100).take_last(5).take(3)),
             std::vector{96, 97, 98});
  }
  SECTION("observable") {
    check_eq(collect(range(1, 100).as_observable().take_last(5).take(3)),
             std::vector{96, 97, 98});
  }
}

TEST("take_last operator forwards errors") {
  SECTION("blueprint") {
    check_eq(collect(obs_error<int>().take_last(5)),
             make_error(sec::runtime_error));
  }
  SECTION("observable") {
    check_eq(collect(obs_error<int>().as_observable().take_last(5)),
             make_error(sec::runtime_error));
  }
}

} // WITH_FIXTURE(test::fixture::flow)
