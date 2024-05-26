// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/test/caf_test_main.hpp"
#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/fixture/flow.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/actor_from_state.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/scheduled_actor/flow.hpp"

using namespace caf;

namespace {

struct concat_map_adder_state {
  static inline const char* name = "adder";

  explicit concat_map_adder_state(int32_t x) : x(x) {
    // nop
  }

  caf::behavior make_behavior() {
    return {
      [this](int32_t y) { return x + y; },
    };
  }

  int32_t x;
};

struct fixture : test::fixture::flow, test::fixture::deterministic {};

WITH_FIXTURE(fixture) {

SCENARIO("concat_map merges multiple observables") {
  using i32_list = std::vector<int32_t>;
  GIVEN("a generation that emits lists for concat_map") {
    WHEN("lifting each list to an observable with concat_map") {
      THEN("the observer receives values from all observables one by one") {
        auto outputs = i32_list{};
        auto inputs = std::vector<i32_list>{
          i32_list{1},
          i32_list{2, 2},
          i32_list{3, 3, 3},
        };
        auto expected_outputs = i32_list{1, 2, 2, 3, 3, 3};
        check_eq(collect(make_observable().from_container(inputs).concat_map(
                   [this](const i32_list& x) {
                     return make_observable().from_container(x);
                   })),
                 expected_outputs);
      }
    }
  }
  GIVEN("a generation that emits 10 integers for concat_map") {
    WHEN("sending a request for each each integer for concat_map") {
      THEN("concat_map merges the responses one by one") {
        auto outputs = i32_list{};
        auto adder = sys.spawn(actor_from_state<concat_map_adder_state>, 1);
        auto [self, launch] = sys.spawn_inactive();
        auto inputs = i32_list(10);
        std::iota(inputs.begin(), inputs.end(), 0);
        self->make_observable()
          .from_container(inputs)
          .concat_map([self = self, adder](int32_t x) {
            return self->mail(x)
              .request(adder, infinite)
              .as_observable<int32_t>();
          })
          .for_each([&outputs](int32_t x) { outputs.emplace_back(x); });
        launch();
        dispatch_messages();
        auto expected_outputs = i32_list{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        check_eq(outputs, expected_outputs);
      }
    }
  }
}

} // WITH_FIXTURE(fixture)

} // namespace
