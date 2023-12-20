// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

// Unlike the other test suites, this one does not focus on a single operator.
// Instead, this test suite uses the API to solve some higher level problems to
// exercise a larger chunk of the API all at once.

#include "caf/test/caf_test_main.hpp"
#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/fixture/flow.hpp"
#include "caf/test/nil.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/event_based_actor.hpp"
#include "caf/scheduled_actor/flow.hpp"

using std::vector;

using namespace caf;
using namespace caf::flow;

namespace {

WITH_FIXTURE(test::fixture::flow) {

TEST("sum up all the multiples of 3 or 5 below 1000") {
  SECTION("solution 1") {
    using snk_t = auto_observer<int>;
    auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
    make_observable()
      .range(1, 999)
      .filter([](int x) { return x % 3 == 0 || x % 5 == 0; })
      .sum()
      .subscribe(snk->as_observer());
    run_flows();
    check_eq(snk->buf, vector{233'168});
    check_eq(snk->state, observer_state::completed);
  }
  SECTION("solution 2") {
    using snk_t = auto_observer<int>;
    auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
    make_observable()
      .merge(make_observable()
               .iota(1)
               .map([](int x) { return x * 3; })
               .take_while([](int x) { return x < 1'000; }),
             make_observable()
               .iota(1)
               .map([](int x) { return x * 5; })
               .take_while([](int x) { return x < 1'000; }))
      .distinct()
      .sum()
      .subscribe(snk->as_observer());
    run_flows();
    check_eq(snk->buf, vector{233'168});
    check_eq(snk->state, observer_state::completed);
  }
}

} // WITH_FIXTURE(test::fixture::flow)

WITH_FIXTURE(test::fixture::deterministic) {

TEST("GH-1399 regression") {
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
  dispatch_messages();
  check_eq(*results, vector{-1, -2, -3, -4, -5, -6, -7, -8, -9, -10});
}

} // WITH_FIXTURE(test::fixture::deterministic)

} // namespace
