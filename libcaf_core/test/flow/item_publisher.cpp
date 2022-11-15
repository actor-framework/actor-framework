// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE flow.item_publisher

#include "caf/flow/item_publisher.hpp"

#include "core-test.hpp"

#include "caf/flow/observable_builder.hpp"
#include "caf/flow/op/merge.hpp"
#include "caf/flow/scoped_coordinator.hpp"

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

SCENARIO("item publishers discard items that arrive before a subscriber") {
  GIVEN("an item publisher") {
    WHEN("publishing items") {
      THEN("observers see only items that were published after subscribing") {
        auto uut = flow::item_publisher<int>{ctx.get()};
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
}

END_FIXTURE_SCOPE()
