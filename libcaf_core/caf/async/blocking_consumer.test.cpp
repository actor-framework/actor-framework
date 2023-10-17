// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/async/blocking_consumer.hpp"

#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/async/blocking_producer.hpp"
#include "caf/scheduled_actor/flow.hpp"

#include <mutex>

using namespace caf;

namespace {

struct fixture {
  actor_system_config cfg;
  actor_system sys;

  fixture()
    : sys(cfg.set("caf.scheduler.max-threads", 2)
            .set("caf.scheduler.policy", "sharing")) {
    // nop
  }
};

void produce(async::producer_resource<int> push) {
  async::blocking_producer out{push.try_open()};
  for (int i = 0; i < 5000; ++i)
    out.push(i);
}

WITH_FIXTURE(fixture) {

SCENARIO("blocking consumers allow threads to receive data") {
  GIVEN("a producers running in a separate thread") {
    WHEN("consuming the generated value with a blocking consumer") {
      THEN("the consumer receives all values in order") {
        auto [pull, push] = async::make_spsc_buffer_resource<int>();
        std::thread producer{produce, push};
        async::blocking_consumer<int> in{pull.try_open()};
        std::vector<int> got;
        bool done = false;
        while (!done) {
          int tmp = 0;
          switch (in.pull(async::delay_errors, tmp)) {
            case async::read_result::ok:
              got.push_back(tmp);
              break;
            case async::read_result::stop:
              done = true;
              break;
            case async::read_result::abort:
              fail("did not expect async::read_result::abort");
              done = true;
              break;
            default:
              fail("unexpected pull result");
              done = true;
          }
        }
        auto want = std::vector<int>(5000);
        std::iota(want.begin(), want.end(), 0);
        check_eq(got.size(), 5000u);
        check_eq(got, want);
        producer.join();
      }
    }
  }
}

} // WITH_FIXTURE(fixture)

} // namespace
