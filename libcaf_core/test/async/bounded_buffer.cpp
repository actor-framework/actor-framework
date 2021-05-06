// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE async.bounded_buffer

#include "caf/async/bounded_buffer.hpp"

#include "core-test.hpp"

#include <memory>

#include "caf/flow/coordinator.hpp"
#include "caf/flow/merge.hpp"
#include "caf/flow/observable_builder.hpp"
#include "caf/flow/observer.hpp"
#include "caf/scheduled_actor/flow.hpp"

using namespace caf;

BEGIN_FIXTURE_SCOPE(test_coordinator_fixture<>)

SCENARIO("bounded buffers moves data between actors") {
  GIVEN("a bounded buffer resource") {
    WHEN("opening the resource from two actors") {
      THEN("data travels through the bounded buffer") {
        using actor_t = event_based_actor;
        auto [rd, wr] = async::make_bounded_buffer_resource<int>(6, 2);
        auto inputs = std::vector<int>{1, 2, 4, 8, 16, 32, 64, 128};
        auto outputs = std::vector<int>{};
        sys.spawn([wr{wr}, &inputs](actor_t* src) {
          src->make_observable()
            .from_container(inputs)
            .filter([](int) { return true; })
            .subscribe(wr);
        });
        sys.spawn([rd{rd}, &outputs](actor_t* snk) {
          snk
            ->make_observable() //
            .from_resource(rd)
            .for_each([&outputs](int x) { outputs.emplace_back(x); });
        });
        run();
        CHECK_EQ(inputs, outputs);
      }
    }
  }
}

END_FIXTURE_SCOPE()
