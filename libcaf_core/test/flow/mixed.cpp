// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

// Unlike the other test suites, this one does not focus on a single operator.
// Instead, this test suite uses the API to solve some higher level problems to
// exercise a larger chunk of the API all at once.
#define CAF_SUITE flow.mixed

#include "core-test.hpp"

#include "caf/flow/observable.hpp"
#include "caf/flow/observable_builder.hpp"
#include "caf/flow/scoped_coordinator.hpp"

using namespace caf;

using caf::flow::make_observer;

namespace {

struct fixture : test_coordinator_fixture<> {
  flow::scoped_coordinator_ptr ctx = flow::make_scoped_coordinator();

  template <class... Ts>
  std::vector<int> ls(Ts... xs) {
    return std::vector<int>{xs...};
  }
};

} // namespace

#define SUB_CASE(text)                                                         \
  MESSAGE(text);                                                               \
  if (true)

BEGIN_FIXTURE_SCOPE(fixture)

SCENARIO("sum up all the multiples of 3 or 5 below 1000") {
  SUB_CASE("solution 1") {
    auto snk = flow::make_auto_observer<int>();
    ctx->make_observable()
      .range(1, 999)
      .filter([](int x) { return x % 3 == 0 || x % 5 == 0; })
      .sum()
      .subscribe(snk->as_observer());
    ctx->run();
    CHECK_EQ(snk->state, flow::observer_state::completed);
    CHECK_EQ(snk->buf, ls(233'168));
  }
  SUB_CASE("solution 2") {
    auto snk = flow::make_auto_observer<int>();
    ctx->make_observable()
      .merge(ctx->make_observable()
               .iota(1)
               .map([](int x) { return x * 3; })
               .take_while([](int x) { return x < 1'000; }),
             ctx->make_observable()
               .iota(1)
               .map([](int x) { return x * 5; })
               .take_while([](int x) { return x < 1'000; }))
      .distinct()
      .sum()
      .subscribe(snk->as_observer());
    ctx->run();
    CHECK_EQ(snk->state, flow::observer_state::completed);
    CHECK_EQ(snk->buf, ls(233'168));
  }
}

END_FIXTURE_SCOPE()
