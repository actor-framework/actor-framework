// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE flow.concat

#include "caf/flow/concat.hpp"

#include "core-test.hpp"

#include "caf/flow/observable_builder.hpp"
#include "caf/flow/scoped_coordinator.hpp"

using namespace caf;

namespace {

struct fixture : test_coordinator_fixture<> {
  flow::scoped_coordinator_ptr ctx = flow::make_scoped_coordinator();
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

SCENARIO("concat operators combine inputs") {
  GIVEN("two observables") {
    WHEN("merging them to a single publisher via concat") {
      THEN("the observer receives the output of both sources in order") {
        auto outputs = std::vector<int>{};
        auto r1 = ctx->make_observable().repeat(11).take(113);
        auto r2 = ctx->make_observable().repeat(22).take(223);
        flow::concat(std::move(r1), std::move(r2)).for_each([&outputs](int x) {
          outputs.emplace_back(x);
        });
        ctx->run();
        if (CHECK_EQ(outputs.size(), 336u)) {
          CHECK(std::all_of(outputs.begin(), outputs.begin() + 113,
                            [](int x) { return x == 11; }));
          CHECK(std::all_of(outputs.begin() + 113, outputs.end(),
                            [](int x) { return x == 22; }));
        }
      }
    }
  }
}

END_FIXTURE_SCOPE()
