// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/test/fixture/flow.hpp"
#include "caf/test/scenario.hpp"

using namespace caf;

namespace {} // namespace

WITH_FIXTURE(test::fixture::flow) {

SCENARIO("filter_map filters and maps values") {
  GIVEN("a list of integers") {
    WHEN("filtering out odd ones with filter_map") {
      THEN("the observer receives only even values") {
        const auto outputs
          = collect(make_observable().iota(0).take(10).filter_map(
            [](int x) -> std::optional<int> {
              return x % 2 == 0 ? std::make_optional(x) : std::nullopt;
            }));
        require(outputs.has_value());
        const auto expected_outputs = std::vector<int>{0, 2, 4, 6, 8};
        check_eq(*outputs, expected_outputs);
      }
    }
  }
}

} // WITH_FIXTURE(test::fixture::flow)
