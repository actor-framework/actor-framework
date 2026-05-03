// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/async/blocking_producer.hpp"

#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/async/consumer.hpp"
#include "caf/async/policy.hpp"
#include "caf/async/spsc_buffer.hpp"
#include "caf/detail/atomic_ref_count.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/init_global_meta_objects.hpp"
#include "caf/make_counted.hpp"
#include "caf/scheduled_actor/flow.hpp"
#include "caf/scoped_actor.hpp"

#include <mutex>
#include <thread>
#include <vector>

// Some offset to avoid collisions with other type IDs.
CAF_BEGIN_TYPE_ID_BLOCK(blocking_producer_test, caf::first_custom_type_id + 170,
                        10)
  CAF_ADD_TYPE_ID(blocking_producer_test, (caf::cow_vector<int>) )
CAF_END_TYPE_ID_BLOCK(blocking_producer_test)

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

struct thread_pull_consumer : async::consumer {
  void on_producer_ready() override {
    // nop
  }

  void on_producer_wakeup() override {
    // nop
  }

  void ref() const noexcept final {
    ref_count_.inc();
  }

  void deref() const noexcept final {
    ref_count_.dec(this);
  }

  mutable caf::detail::atomic_ref_count ref_count_;
};

struct int_observer {
  void on_next(const int& x) {
    out->push_back(x);
  }

  void on_error(const error&) {
    // nop
  }

  void on_complete() {
    // nop
  }

  std::vector<int>* out = nullptr;
};

void do_push(sync_t* sync, push_val_t push, int begin, int end) {
  auto out = async::make_blocking_producer(std::move(push));
  if (!out) {
    CAF_RAISE_ERROR("make_blocking_producer");
  }
  for (auto i = begin; i != end; ++i)
    out->push(i);
  sync->release();
}

std::pair<std::thread, pull_val_t> start_worker(sync_t* sync, int begin,
                                                int end) {
  auto [pull, push] = async::make_spsc_buffer_resource<int>();
  return {std::thread{do_push, sync, std::move(push), begin, end},
          std::move(pull)};
}

void run(push_resource_t push) {
  auto out = async::make_blocking_producer(std::move(push));
  if (!out) {
    CAF_RAISE_ERROR("make_blocking_producer");
  }
  sync_t sync;
  std::vector<std::thread> threads;
  auto add_worker = [&](int begin, int end) {
    auto [hdl, pull] = start_worker(&sync, begin, end);
    threads.push_back(std::move(hdl));
    out->push(std::move(pull));
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

} // namespace

WITH_FIXTURE(fixture) {

TEST("blocking producer waits when the buffer is at hard capacity") {
  auto [rd, wr] = async::make_spsc_buffer_resource<int>(2, 1);
  auto wbuf = wr.try_open();
  require_ne(wbuf, nullptr);
  auto rbuf = rd.try_open();
  require_ne(rbuf, nullptr);
  auto cons = make_counted<thread_pull_consumer>();
  rbuf->set_consumer(cons);
  async::blocking_producer<int> out{std::move(wbuf)};
  std::vector<int> received;
  int_observer obs{&received};
  // Run pushes on a worker thread and pull from the main thread so a
  // spinning consumer cannot starve the producer (and vice versa: retry with
  // yield when `pull` returns no data yet — an open buffer may report
  // `{true, 0}` until the producer enqueues).
  std::thread producer([&] {
    for (int i = 0; i < 10; ++i)
      check(out.push(i));
  });
  while (received.size() < 10u) {
    auto [ok, n] = rbuf->pull(async::delay_errors, 1, obs);
    if (!ok)
      break;
    if (n == 0)
      std::this_thread::yield();
  }
  producer.join();
  if (check_eq(received.size(), 10u)) {
    for (int i = 0; i < 10; ++i)
      check_eq(received[static_cast<size_t>(i)], i);
  }
  rbuf->cancel();
}

SCENARIO("blocking producers allow threads to generate data") {
  GIVEN("a dynamic set of blocking producers") {
    WHEN("consuming the generated values from an actor via flat_map") {
      THEN("the actor merges all values from all buffers into one") {
        auto [pull, push] = async::make_spsc_buffer_resource<pull_val_t>();
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
  init_global_meta_objects<id_block::blocking_producer_test>();
}
