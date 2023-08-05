// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE flow.single

#include "caf/flow/single.hpp"

#include "caf/flow/op/cell.hpp"
#include "caf/flow/scoped_coordinator.hpp"

#include "core-test.hpp"

using namespace caf;

namespace {

struct fixture : test_coordinator_fixture<> {
  flow::scoped_coordinator_ptr ctx = flow::make_scoped_coordinator();
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(single_tests, fixture)

SCENARIO("singles emit at most one value") {
  using i32_list = std::vector<int32_t>;
  GIVEN("a single<int32>") {
    WHEN("an observer subscribes before the values has been set") {
      THEN("the observer receives the value when calling set_value") {
        auto outputs = i32_list{};
        auto cell = make_counted<flow::op::cell<int32_t>>(ctx.get());
        auto single_int = flow::single<int32_t>{cell};
        single_int //
          .as_observable()
          .for_each([&outputs](int32_t x) { outputs.emplace_back(x); });
        ctx->run();
        CHECK_EQ(outputs, i32_list());
        cell->set_value(42);
        CHECK_EQ(outputs, i32_list({42}));
      }
    }
    WHEN("an observer subscribes after the values has been set") {
      THEN("the observer receives the value immediately") {
        auto outputs = i32_list{};
        auto cell = make_counted<flow::op::cell<int32_t>>(ctx.get());
        auto single_int = flow::single<int32_t>{cell};
        cell->set_value(42);
        single_int //
          .as_observable()
          .for_each([&outputs](int32_t x) { outputs.emplace_back(x); });
        ctx->run();
        CHECK_EQ(outputs, i32_list({42}));
      }
    }
  }
}

CAF_TEST_FIXTURE_SCOPE_END()
