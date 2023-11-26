// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/caf_test_main.hpp"
#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/fixture/flow.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

using namespace caf;

namespace {

WITH_FIXTURE(test::fixture::flow) {

SCENARIO("for_each iterates all values in a stream") {
  GIVEN("a generation") {
    WHEN("subscribing to its output via for_each") {
      THEN("the observer receives all values") {
        /* subtest */ {
          auto inputs = std::vector<int>{1, 2, 4, 8, 16, 32, 64, 128};
          auto outputs = std::vector<int>{};
          make_observable()
            .from_container(inputs) //
            .filter([](int) { return true; })
            .for_each([&outputs](int x) { outputs.emplace_back(x); });
          run_flows();
          check_eq(inputs, outputs);
        }
        /* subtest */ {
          auto inputs = std::vector<int>{21, 21, 21, 21, 21, 21, 21};
          auto outputs = std::vector<int>{};
          make_observable()
            .repeat(7) //
            .take(7)
            .map([](int x) { return x * 3; })
            .for_each([&outputs](int x) { outputs.emplace_back(x); });
          run_flows();
          check_eq(inputs, outputs);
        }
      }
    }
  }
  GIVEN("a transformation") {
    WHEN("subscribing to its output via for_each") {
      THEN("the observer receives all values") {
        /* subtest */ {
          auto inputs = std::vector<int>{1, 2, 4, 8, 16, 32, 64, 128};
          auto outputs = std::vector<int>{};
          make_observable()
            .from_container(inputs) //
            .as_observable()
            .filter([](int) { return true; })
            .for_each([&outputs](int x) { outputs.emplace_back(x); });
          run_flows();
          check_eq(inputs, outputs);
        }
        /* subtest */ {
          auto completed = false;
          auto inputs = std::vector<int>{21, 21, 21, 21, 21, 21, 21};
          auto outputs = std::vector<int>{};
          make_observable()
            .repeat(7) //
            .as_observable()
            .take(7)
            .map([](int x) { return x * 3; })
            .do_on_error([this](const error& err) {
              fail("on_error: {}", caf::to_string(err));
            })
            .do_on_complete([&completed] { completed = true; })
            .for_each([&outputs](int x) { outputs.emplace_back(x); });
          run_flows();
          check(completed);
          check_eq(inputs, outputs);
        }
      }
    }
  }
  GIVEN("an observable") {
    WHEN("subscribing to its output via for_each") {
      THEN("the observer receives all values") {
        /* subtest */ {
          auto inputs = std::vector<int>{1, 2, 4, 8, 16, 32, 64, 128};
          auto outputs = std::vector<int>{};
          make_observable()
            .from_container(inputs) //
            .filter([](int) { return true; })
            .as_observable()
            .for_each([&outputs](int x) { outputs.emplace_back(x); });
          run_flows();
          check_eq(inputs, outputs);
        }
        /* subtest */ {
          auto inputs = std::vector<int>{21, 21, 21, 21, 21, 21, 21};
          auto outputs = std::vector<int>{};
          make_observable()
            .repeat(7) //
            .take(7)
            .map([](int x) { return x * 3; })
            .as_observable()
            .for_each([&outputs](int x) { outputs.emplace_back(x); });
          run_flows();
          check_eq(inputs, outputs);
        }
      }
    }
  }
}

} // WITH_FIXTURE(test::fixture::flow)

} // namespace
