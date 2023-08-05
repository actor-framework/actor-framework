// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

// Unlike the other test suites, this one does not focus on a single operator.
// Instead, this test suite uses the API to solve some higher level problems to
// exercise a larger chunk of the API all at once.
#define CAF_SUITE flow.mixed

#include "caf/flow/observable.hpp"
#include "caf/flow/observable_builder.hpp"
#include "caf/flow/scoped_coordinator.hpp"
#include "caf/scheduled_actor/flow.hpp"

#include "core-test.hpp"

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

TEST_CASE("GH-1399 regression") {
  // Original issue: flat_map does not limit the demand it signals upstream.
  // When running flat_map on an unbound sequence like iota-observable, it
  // produces an infinite amount of observables without ever giving downstream
  // operators the opportunity to cut off the flow items.
  auto worker_fn = []() -> behavior {
    return {
      [](int x) { return -x; },
    };
  };
  auto worker = sys.spawn(worker_fn);
  auto results = std::make_shared<std::vector<int>>();
  auto run_fn = [worker, results](caf::event_based_actor* self) {
    self->make_observable()
      .iota(1)
      .flat_map([self, worker](int x) {
        return self->request(worker, infinite, x).as_observable<int32_t>();
      })
      .take(10)
      .for_each([results](int value) { results->push_back(value); });
  };
  sys.spawn(run_fn);
  run();
  CHECK_EQ(*results, ls(-1, -2, -3, -4, -5, -6, -7, -8, -9, -10));
}

END_FIXTURE_SCOPE()
