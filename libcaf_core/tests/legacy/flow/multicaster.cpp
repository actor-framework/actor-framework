// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE flow.multicaster

#include "caf/flow/multicaster.hpp"

#include "caf/flow/observable_builder.hpp"
#include "caf/flow/op/merge.hpp"
#include "caf/flow/scoped_coordinator.hpp"

#include "core-test.hpp"

using namespace caf;

namespace {

struct fixture : test_coordinator_fixture<> {
  flow::scoped_coordinator_ptr ctx = flow::make_scoped_coordinator();

  template <class... Ts>
  std::vector<int> ls(Ts... xs) {
    return std::vector<int>{xs...};
  }
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

SCENARIO("a multicaster pushes items to all subscribers") {
  GIVEN("a multicaster with two subscribers") {
    auto uut = flow::multicaster<int>{ctx.get()};
    auto snk1 = flow::make_passive_observer<int>();
    auto snk2 = flow::make_passive_observer<int>();
    uut.subscribe(snk1->as_observer());
    uut.subscribe(snk2->as_observer());
    CHECK(uut.impl().has_observers());
    CHECK_EQ(uut.impl().observer_count(), 2u);
    WHEN("pushing items") {
      THEN("all observers see all items") {
        CHECK_EQ(uut.impl().min_demand(), 0u);
        CHECK_EQ(uut.impl().max_demand(), 0u);
        CHECK_EQ(uut.impl().min_buffered(), 0u);
        CHECK_EQ(uut.impl().max_buffered(), 0u);

        auto print_stats = [&] {
          const auto& states = uut.impl().observers();
          for (size_t index = 0; index < states.size(); ++index) {
            printf("observer %d: buf size = %d; demand = %d\n", (int) index,
                   (int) states[index]->buf.size(),
                   (int) states[index]->demand);
          }
        };
        print_stats();

        // Push 3 items with no demand.
        CHECK_EQ(uut.push({1, 2, 3}), 0u);
        ctx->run();
        print_stats();
        CHECK_EQ(uut.impl().min_demand(), 0u);
        CHECK_EQ(uut.impl().max_demand(), 0u);
        CHECK_EQ(uut.impl().min_buffered(), 3u);
        CHECK_EQ(uut.impl().max_buffered(), 3u);
        CHECK_EQ(snk1->buf, ls());
        CHECK_EQ(snk2->buf, ls());
        // Pull out one item with snk1.
        snk1->sub.request(1);
        ctx->run();
        CHECK_EQ(uut.impl().min_demand(), 0u);
        CHECK_EQ(uut.impl().max_demand(), 0u);
        CHECK_EQ(uut.impl().min_buffered(), 2u);
        CHECK_EQ(uut.impl().max_buffered(), 3u);
        CHECK_EQ(snk1->buf, ls(1));
        CHECK_EQ(snk2->buf, ls());
        // Pull out all item with snk1 plus 2 extra demand.
        snk1->sub.request(4);
        ctx->run();
        CHECK_EQ(uut.impl().min_demand(), 0u);
        CHECK_EQ(uut.impl().max_demand(), 2u);
        CHECK_EQ(uut.impl().min_buffered(), 0u);
        CHECK_EQ(uut.impl().max_buffered(), 3u);
        CHECK_EQ(snk1->buf, ls(1, 2, 3));
        CHECK_EQ(snk2->buf, ls());
        // Pull out all items with snk2 plus 4 extra demand.
        snk2->sub.request(7);
        ctx->run();
        CHECK_EQ(uut.impl().min_demand(), 2u);
        CHECK_EQ(uut.impl().max_demand(), 4u);
        CHECK_EQ(uut.impl().min_buffered(), 0u);
        CHECK_EQ(uut.impl().max_buffered(), 0u);
        CHECK_EQ(snk1->buf, ls(1, 2, 3));
        CHECK_EQ(snk2->buf, ls(1, 2, 3));
        // Push 3 more items, expect 2 to be dispatched immediately.
        CHECK_EQ(uut.push({4, 5, 6}), 2u);
        CHECK_EQ(uut.impl().min_demand(), 0u);
        CHECK_EQ(uut.impl().max_demand(), 1u);
        CHECK_EQ(uut.impl().min_buffered(), 0u);
        CHECK_EQ(uut.impl().max_buffered(), 1u);
        CHECK_EQ(snk1->buf, ls(1, 2, 3, 4, 5));
        CHECK_EQ(snk2->buf, ls(1, 2, 3, 4, 5, 6));
        // Pull out the remaining element with snk1 plus 9 extra demand.
        snk1->sub.request(10);
        ctx->run();
        CHECK_EQ(uut.impl().min_demand(), 1u);
        CHECK_EQ(uut.impl().max_demand(), 9u);
        CHECK_EQ(uut.impl().min_buffered(), 0u);
        CHECK_EQ(uut.impl().max_buffered(), 0u);
        CHECK_EQ(snk1->buf, ls(1, 2, 3, 4, 5, 6));
        CHECK_EQ(snk2->buf, ls(1, 2, 3, 4, 5, 6));
        // Close: must call on_complete immediately since buffers are empty.
        uut.close();
        CHECK_EQ(snk1->state, flow::observer_state::completed);
        CHECK_EQ(snk2->state, flow::observer_state::completed);
      }
    }
  }
}

SCENARIO("a multicaster discards items that arrive before a subscriber") {
  WHEN("pushing items") {
    THEN("observers see only items that were pushed after subscribing") {
      auto uut = flow::multicaster<int>{ctx.get()};
      uut.push({1, 2, 3});
      auto snk = flow::make_auto_observer<int>();
      uut.subscribe(snk->as_observer());
      ctx->run();
      uut.push({4, 5, 6});
      ctx->run();
      uut.close();
      CHECK_EQ(snk->buf, ls(4, 5, 6));
      CHECK_EQ(snk->state, flow::observer_state::completed);
    }
  }
}

END_FIXTURE_SCOPE()
