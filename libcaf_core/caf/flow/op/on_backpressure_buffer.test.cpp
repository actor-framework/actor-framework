// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/flow/op/on_backpressure_buffer.hpp"

#include "caf/test/fixture/flow.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/flow/multicaster.hpp"

using namespace caf;

namespace {

WITH_FIXTURE(test::fixture::flow) {

SCENARIO("the backpressure operator is transparent with sufficient demand") {
  GIVEN("a backpressure buffer") {
    WHEN("the observer always signals sufficient demand") {
      THEN("the buffer is transparent and all items are delivered") {
        auto uut = range(1, 99).on_backpressure_buffer(10);
        auto obs = make_auto_observer<int>();
        uut.subscribe(obs->as_observer());
        run_flows();
        auto want = std::vector<int>{};
        for (int i = 1; i <= 99; ++i)
          want.push_back(i);
        check_eq(obs->buf, want);
      }
    }
    WHEN("the observer always signals new demand before the buffer overflows") {
      THEN("the buffer is transparent and all items are delivered") {
        auto mcast = caf::flow::multicaster<int>{coordinator()};
        auto obs = make_passive_observer<int>();
        mcast
          .as_observable() //
          .on_backpressure_buffer(10)
          .subscribe(obs->as_observer());
        run_flows();
        // Push 10 items, 5 can be delivered immediately.
        obs->sub.request(5);
        for (int i = 1; i <= 10; ++i)
          mcast.push(i);
        run_flows();
        check_eq(obs->buf, std::vector{1, 2, 3, 4, 5});
        // Request the remaining items.
        obs->sub.request(2); // Triggers a call to on_request.
        check_eq(pending_actions(), 1u);
        obs->sub.request(13); // Only increases demand.
        check_eq(pending_actions(), 1u);
        run_flows();
        check_eq(obs->buf, std::vector{1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
      }
    }
  }
}

SCENARIO("the drop newest strategy discards items when the buffer is full") {
  GIVEN("a backpressure buffer with a drop newest strategy") {
    auto strategy = caf::flow::backpressure_overflow_strategy::drop_newest;
    WHEN("the buffer is full") {
      THEN("the newest item gets discarded") {
        auto uut = range(1, 99).on_backpressure_buffer(10, strategy);
        auto obs = make_passive_observer<int>();
        uut.subscribe(obs->as_observer());
        run_flows();
        obs->sub.request(100);
        run_flows();
        check_eq(obs->buf, std::vector{1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
      }
    }
  }
}

SCENARIO("the drop oldest strategy discards items when the buffer is full") {
  GIVEN("a backpressure buffer with a drop oldest strategy") {
    auto strategy = caf::flow::backpressure_overflow_strategy::drop_oldest;
    WHEN("the buffer is full") {
      THEN("the oldest item gets discarded") {
        auto uut = range(1, 99).on_backpressure_buffer(10, strategy);
        auto obs = make_passive_observer<int>();
        uut.subscribe(obs->as_observer());
        run_flows();
        obs->sub.request(100);
        run_flows();
        check_eq(obs->buf, std::vector{90, 91, 92, 93, 94, 95, 96, 97, 98, 99});
      }
    }
  }
}

SCENARIO("the fail strategy aborts the flow when the buffer is full") {
  GIVEN("a backpressure buffer with a drop oldest strategy") {
    auto strategy = caf::flow::backpressure_overflow_strategy::fail;
    WHEN("the buffer is full") {
      THEN("the flow aborts") {
        auto uut = range(1, 99).on_backpressure_buffer(10, strategy);
        auto obs = make_passive_observer<int>();
        uut.subscribe(obs->as_observer());
        run_flows();
        check_eq(obs->err, sec::backpressure_overflow);
      }
    }
  }
}

TEST("on_complete") {
  SECTION("empty input observable") {
    auto obs = make_passive_observer<int>();
    make_observable()
      .empty<int>() //
      .on_backpressure_buffer(10)
      .subscribe(obs->as_observer());
    run_flows();
    check_eq(obs->state, observer_state::completed);
  }
  SECTION("delayed completion until buffer is empty") {
    auto obs = make_passive_observer<int>();
    just(1).on_backpressure_buffer(10).subscribe(obs->as_observer());
    run_flows();
    check(obs->buf.empty());
    check_eq(obs->state, observer_state::subscribed);
    obs->sub.request(1);
    run_flows();
    check_eq(obs->buf, std::vector{1});
    check_eq(obs->state, observer_state::completed);
  }
}

TEST("on_error") {
  SECTION("fail input observable") {
    auto obs = make_passive_observer<int>();
    obs_error<int>().on_backpressure_buffer(10).subscribe(obs->as_observer());
    run_flows();
    check_eq(obs->state, observer_state::aborted);
  }
  SECTION("delayed on_error until buffer is empty") {
    auto obs = make_passive_observer<int>();
    just(1)
      .concat(make_observable().fail<int>(sec::runtime_error))
      .on_backpressure_buffer(10)
      .subscribe(obs->as_observer());
    run_flows();
    check(obs->buf.empty());
    check_eq(obs->state, observer_state::subscribed);
    obs->sub.request(1);
    run_flows();
    check_eq(obs->buf, std::vector{1});
    check_eq(obs->state, observer_state::aborted);
  }
}

TEST("cancel") {
  auto obs = make_canceling_observer<int>(true);
  range(1, 5).on_backpressure_buffer(10).subscribe(obs->as_observer());
  run_flows();
  check_eq(obs->buf, std::vector{1});
}

TEST("external dispose") {
  auto obs = make_passive_observer<int>();
  auto hdl = just(1).on_backpressure_buffer(10).subscribe(obs->as_observer());
  run_flows();
  check(obs->buf.empty());
  check_eq(obs->state, observer_state::subscribed);
  hdl.dispose();
  run_flows();
  check_eq(obs->state, observer_state::aborted);
}

} // WITH_FIXTURE(test::fixture::flow)

} // namespace
