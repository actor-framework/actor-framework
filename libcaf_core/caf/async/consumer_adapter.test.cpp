// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/async/consumer_adapter.hpp"

#include "caf/test/scenario.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/async/blocking_producer.hpp"
#include "caf/flow/scoped_coordinator.hpp"
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

class runner_t {
public:
  runner_t(async::spsc_buffer_ptr<int> buf, async::execution_context_ptr ctx) {
    do_wakeup_ = make_action([this] { resume(); });
    in_ = make_consumer_adapter(buf, ctx, do_wakeup_);
  }

  disposable as_disposable() {
    return do_wakeup_.as_disposable();
  }

  void resume() {
    int tmp = 0;
    for (;;) {
      auto res = in_.pull(async::delay_errors, tmp);
      switch (res) {
        case async::read_result::ok:
          values_.push_back(tmp);
          break;
        case async::read_result::stop:
          do_wakeup_.dispose();
          return;
        case async::read_result::try_again_later:
          return;
        default:
          test::runnable::current().fail("unexpected pull result: {}",
                                         to_string(res));
          do_wakeup_.dispose();
          return;
      }
    }
  }

  void start() {
    resume();
  }

  const std::vector<int>& values() {
    return values_;
  }

private:
  async::consumer_adapter<int> in_;
  action do_wakeup_;
  std::vector<int> values_;
};

} // namespace

WITH_FIXTURE(fixture) {

SCENARIO("consumer adapters allow integrating consumers into event loops") {
  GIVEN("a producers running in a separate thread") {
    WHEN("consuming the generated value with a blocking consumer") {
      THEN("the consumer receives all values in order") {
        auto [pull, push] = async::make_spsc_buffer_resource<int>();
        std::thread producer{produce, push};
        auto loop = flow::make_scoped_coordinator();
        runner_t runner{pull.try_open(), loop};
        loop->watch(runner.as_disposable());
        runner.start();
        loop->run();
        auto& got = runner.values();
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
