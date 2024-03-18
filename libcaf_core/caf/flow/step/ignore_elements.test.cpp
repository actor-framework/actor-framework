// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/flow/step/ignore_elements.hpp"

#include "caf/test/fixture/flow.hpp"
#include "caf/test/test.hpp"

#include "caf/flow/scoped_coordinator.hpp"
#include "caf/scheduled_actor/flow.hpp"

#include <vector>

using namespace caf;

namespace {

WITH_FIXTURE(test::fixture::flow) {

TEST("calling ignore_elements on range(1, 10) produces []") {
  SECTION("blueprint") {
    check_eq(collect(range(1, 10).ignore_elements()), std::vector<int>{});
  }
  SECTION("observable") {
    check_eq(collect(range(1, 10).as_observable().ignore_elements()),
             std::vector<int>{});
  }
}

TEST("ignore_elements operator forwards errors") {
  SECTION("blueprint") {
    check_eq(collect(obs_error<int>().ignore_elements()),
             error{sec::runtime_error});
  }
  SECTION("observable") {
    check_eq(collect(obs_error<int>().as_observable().ignore_elements()),
             error{sec::runtime_error});
  }
}

} // WITH_FIXTURE(test::fixture::flow)

} // namespace
