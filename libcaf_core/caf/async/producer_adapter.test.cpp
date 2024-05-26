// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/async/producer_adapter.hpp"

#include "caf/test/scenario.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/async/blocking_producer.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/flow/scoped_coordinator.hpp"
#include "caf/init_global_meta_objects.hpp"
#include "caf/scheduled_actor/flow.hpp"
#include "caf/scoped_actor.hpp"

#include <mutex>

// Some offset to avoid collisions with other type IDs.
CAF_BEGIN_TYPE_ID_BLOCK(producer_adapter_test, caf::first_custom_type_id + 10)
  CAF_ADD_TYPE_ID(producer_adapter_test, (caf::cow_vector<int>) )
CAF_END_TYPE_ID_BLOCK(producer_adapter_test)

using namespace caf;

namespace {

struct fixture {
  actor_system_config cfg;
  actor_system sys;

  fixture() : sys(cfg.set("caf.scheduler.max-threads", 2)) {
    // nop
  }
};

struct sync_t {
  std::mutex mtx;
  std::condition_variable cv;
  int available = 0;

  void release() {
    std::unique_lock guard{mtx};
    if (++available == 1)
      cv.notify_all();
  }

  void acquire() {
    std::unique_lock guard{mtx};
    while (available == 0)
      cv.wait(guard);
    --available;
  }
};

using push_val_t = async::producer_resource<int>;

using pull_val_t = async::consumer_resource<int>;

using push_resource_t = async::producer_resource<pull_val_t>;

using pull_resource_t = async::consumer_resource<pull_val_t>;

class runner_t {
public:
  runner_t(async::spsc_buffer_ptr<int> buf, async::execution_context_ptr ctx,
           int first, int last)
    : n(first), end(last) {
    do_resume_ = make_action([this] { resume(); });
    do_cancel_ = make_action([this] { do_resume_.dispose(); });
    out = make_producer_adapter(buf, ctx, do_resume_, do_cancel_);
  }

  disposable as_disposable() {
    return do_resume_.as_disposable();
  }

  void resume() {
    while (n < end)
      if (out.push(n++) == 0)
        return;
    do_resume_.dispose();
  }

private:
  int n;
  int end;
  async::producer_adapter<int> out;
  action do_resume_;
  action do_cancel_;
};

void do_push(sync_t* sync, push_val_t push, int begin, int end) {
  auto buf = push.try_open();
  if (!buf) {
    CAF_RAISE_ERROR("push.try_open failed");
  }
  auto loop = flow::make_scoped_coordinator();
  runner_t runner{buf, loop, begin, end};
  loop->watch(runner.as_disposable());
  loop->run();
  sync->release();
}

std::pair<std::thread, pull_val_t> start_worker(sync_t* sync, int begin,
                                                int end) {
  auto [pull, push] = async::make_spsc_buffer_resource<int>();
  return {std::thread{do_push, sync, std::move(push), begin, end},
          std::move(pull)};
}

void run(push_resource_t push) {
  auto buf = push.try_open();
  if (!buf) {
    CAF_RAISE_ERROR("push.try_open failed");
  }
  // Note: using the adapter here as well would be tricky since we would need to
  // wait for available threads.
  async::blocking_producer out{std::move(buf)};
  sync_t sync;
  std::vector<std::thread> threads;
  auto add_worker = [&](int begin, int end) {
    auto [hdl, pull] = start_worker(&sync, begin, end);
    threads.push_back(std::move(hdl));
    out.push(std::move(pull));
  };
  std::vector<std::pair<int, int>> ranges{{0, 1337},    {1337, 1338},
                                          {1338, 1338}, {1338, 2777},
                                          {2777, 3000}, {3000, 3003},
                                          {3003, 3500}, {3500, 4000}};
  add_worker(4000, 4007);
  add_worker(4007, 4333);
  add_worker(4333, 4500);
  add_worker(4500, 5000);
  for (auto [begin, end] : ranges) {
    sync.acquire();
    add_worker(begin, end);
  }
  for (auto& hdl : threads)
    hdl.join();
}

void receiver_impl(event_based_actor* self, pull_resource_t inputs,
                   actor parent) {
  inputs.observe_on(self)
    .flat_map([self](const pull_val_t& in) { return in.observe_on(self); })
    .to_vector()
    .for_each([self, parent](const cow_vector<int>& values) { //
      self->mail(values).send(parent);
    });
}

WITH_FIXTURE(fixture) {

SCENARIO("producers adapters allow integrating producers into event loops") {
  GIVEN("a dynamic set of blocking producers") {
    WHEN("consuming the generated values from an actor via flat_map") {
      THEN("the actor merges all values from all buffers into one") {
        auto [pull, push] = async::make_spsc_buffer_resource<pull_val_t>();
        auto loop = flow::make_scoped_coordinator();

        scoped_actor self{sys};
        auto receiver = self->spawn(receiver_impl, std::move(pull),
                                    actor{self});
        std::thread runner{::run, std::move(push)};
        self->receive([this](const cow_vector<int>& values) {
          auto want = std::vector<int>(5000);
          std::iota(want.begin(), want.end(), 0);
          auto got = values.std();
          std::sort(got.begin(), got.end());
          check_eq(got.size(), 5000u);
          check_eq(got, want);
        });
        runner.join();
      }
    }
  }
}

} // WITH_FIXTURE(fixture)

TEST_INIT() {
  init_global_meta_objects<id_block::producer_adapter_test>();
}

} // namespace
