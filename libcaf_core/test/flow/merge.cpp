// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE flow.merge

#include "caf/flow/merge.hpp"

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

SCENARIO("merge operators combine inputs") {
  GIVEN("two observables") {
    WHEN("merging them to a single publisher") {
      THEN("the observer receives the output of both sources") {
        auto on_complete_called = false;
        auto outputs = std::vector<int>{};
        auto r1 = ctx->make_observable().repeat(11).take(113);
        auto r2 = ctx->make_observable().repeat(22).take(223);
        flow::merge(std::move(r1), std::move(r2))
          .for_each([&outputs](int x) { outputs.emplace_back(x); },
                    [](const error& err) { FAIL("unexpected error:" << err); },
                    [&on_complete_called] { on_complete_called = true; });
        ctx->run();
        CHECK(on_complete_called);
        if (CHECK_EQ(outputs.size(), 336u)) {
          std::sort(outputs.begin(), outputs.end());
          CHECK(std::all_of(outputs.begin(), outputs.begin() + 113,
                            [](int x) { return x == 11; }));
          CHECK(std::all_of(outputs.begin() + 113, outputs.end(),
                            [](int x) { return x == 22; }));
        }
      }
    }
  }
}

SCENARIO("mergers can delay shutdown") {
  GIVEN("a merger with two inputs and shutdown_on_last_complete set to false") {
    WHEN("both inputs completed") {
      THEN("the merger only closes after enabling shutdown_on_last_complete") {
        auto on_complete_called = false;
        auto outputs = std::vector<int>{};
        auto merger = make_counted<flow::merger_impl<int>>(ctx.get());
        merger->shutdown_on_last_complete(false);
        merger->add(ctx->make_observable().repeat(11).take(113));
        merger->add(ctx->make_observable().repeat(22).take(223));
        merger //
          ->as_observable()
          .for_each([&outputs](int x) { outputs.emplace_back(x); },
                    [](const error& err) { FAIL("unexpected error:" << err); },
                    [&on_complete_called] { on_complete_called = true; });
        ctx->run();
        CHECK(!on_complete_called);
        if (CHECK_EQ(outputs.size(), 336u)) {
          std::sort(outputs.begin(), outputs.end());
          CHECK(std::all_of(outputs.begin(), outputs.begin() + 113,
                            [](int x) { return x == 11; }));
          CHECK(std::all_of(outputs.begin() + 113, outputs.end(),
                            [](int x) { return x == 22; }));
        }
        merger->shutdown_on_last_complete(true);
        ctx->run();
        CHECK(on_complete_called);
      }
    }
  }
}

END_FIXTURE_SCOPE()
