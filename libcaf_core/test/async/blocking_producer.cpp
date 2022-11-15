// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE async.blocking_producer

#include "caf/async/blocking_producer.hpp"

#include "core-test.hpp"

#include "caf/scheduled_actor/flow.hpp"

#include <mutex>

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

void do_push(sync_t* sync, push_val_t push, int begin, int end) {
  auto buf = push.try_open();
  if (!buf) {
    CAF_RAISE_ERROR("push.try_open failed");
  }
  async::blocking_producer out{std::move(buf)};
  for (auto i = begin; i != end; ++i)
    out.push(i);
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
      self->send(parent, values);
    });
}

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

SCENARIO("blocking producers allow threads to generate data") {
  GIVEN("a dynamic set of blocking producers") {
    WHEN("consuming the generated values from an actor via flat_map") {
      THEN("the actor merges all values from all buffers into one") {
        auto [pull, push] = async::make_spsc_buffer_resource<pull_val_t>();
        scoped_actor self{sys};
        auto receiver = self->spawn(receiver_impl, std::move(pull),
                                    actor{self});
        std::thread runner{run, std::move(push)};
        self->receive([](const cow_vector<int>& values) {
          auto want = std::vector<int>(5000);
          std::iota(want.begin(), want.end(), 0);
          auto got = values.std();
          std::sort(got.begin(), got.end());
          CHECK_EQ(got.size(), 5000u);
          CHECK_EQ(got, want);
        });
        runner.join();
      }
    }
  }
}

END_FIXTURE_SCOPE()
