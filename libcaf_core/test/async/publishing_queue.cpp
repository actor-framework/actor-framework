// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE async.publishing_queue

#include "caf/async/publishing_queue.hpp"

#include "core-test.hpp"

#include "caf/scheduled_actor/flow.hpp"

using namespace caf;

namespace {

struct fixture {
  actor_system_config cfg;
  actor_system sys;

  fixture() : sys(cfg.set("caf.scheduler.max-threads", 2)) {
    // nop
  }
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

SCENARIO("publishing queues connect asynchronous producers to observers") {
  GIVEN("a producer and a consumer, living in separate threads") {
    WHEN("connecting producer and consumer via a publishing queue") {
      THEN("the consumer receives all produced values in order") {
        auto [queue, src] = async::make_publishing_queue<int>(sys, 100);
        auto producer_thread = std::thread{[q{std::move(queue)}] {
          for (int i = 0; i < 5000; ++i)
            q->push(i);
        }};
        std::vector<int> values;
        auto consumer_thread = std::thread{[&values, src{src}] {
          src.blocking_for_each([&](int x) { values.emplace_back(x); });
        }};
        producer_thread.join();
        consumer_thread.join();
        std::vector<int> expected_values;
        expected_values.resize(5000);
        std::iota(expected_values.begin(), expected_values.end(), 0);
        CHECK_EQ(values, expected_values);
      }
    }
  }
}

END_FIXTURE_SCOPE()
