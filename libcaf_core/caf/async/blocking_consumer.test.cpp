// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

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

} // namespace

WITH_FIXTURE(fixture) {

SCENARIO("blocking consumers allow threads to receive data") {
  GIVEN("a producers running in a separate thread") {
    WHEN("consuming the generated value with a blocking consumer") {
      THEN("the consumer receives all values in order") {
        auto produce = [this](async::producer_resource<int> push) {
          auto out = async::make_blocking_producer(std::move(push));
          if (!out)
            fail("make_blocking_producer failed");
          async::blocking_producer<int> blocking_out = std::move(*out);
          for (int i = 0; i < 5000; ++i)
            blocking_out.push(i);
        };
        auto [pull, push] = async::make_spsc_buffer_resource<int>();
        std::thread producer{produce, push};
        auto in = async::make_blocking_consumer(std::move(pull));
        require(in.has_value());
        async::blocking_consumer<int> blocking_in = std::move(*in);
        std::vector<int> got;
        bool done = false;
        while (!done) {
          int tmp = 0;
          switch (blocking_in.pull(async::delay_errors, tmp)) {
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
