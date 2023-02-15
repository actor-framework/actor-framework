// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE async.spsc_buffer

#include "caf/async/spsc_buffer.hpp"

#include "core-test.hpp"

#include <memory>

#include "caf/flow/coordinator.hpp"
#include "caf/flow/observable_builder.hpp"
#include "caf/flow/observer.hpp"
#include "caf/scheduled_actor/flow.hpp"

using namespace caf;

namespace {

class dummy_producer : public async::producer {
public:
  dummy_producer() = default;
  dummy_producer(const dummy_producer&) = delete;
  dummy_producer& operator=(const dummy_producer&) = delete;

  void on_consumer_ready() {
    consumer_ready = true;
  }

  void on_consumer_cancel() {
    consumer_cancel = true;
  }

  void on_consumer_demand(size_t new_demand) {
    demand += new_demand;
  }

  void ref_producer() const noexcept {
    ++rc;
  }

  void deref_producer() const noexcept {
    if (--rc == 0)
      delete this;
  }

  mutable size_t rc = 1;
  bool consumer_ready = false;
  bool consumer_cancel = false;
  size_t demand = 0;
};

class dummy_consumer : public async::consumer {
public:
  dummy_consumer() = default;
  dummy_consumer(const dummy_consumer&) = delete;
  dummy_consumer& operator=(const dummy_consumer&) = delete;

  void on_producer_ready() {
    producer_ready = true;
  }

  void on_producer_wakeup() {
    ++producer_wakeups;
  }

  void ref_consumer() const noexcept {
    ++rc;
  }

  void deref_consumer() const noexcept {
    if (--rc == 0)
      delete this;
  }

  mutable size_t rc = 1;
  bool producer_ready = false;
  size_t producer_wakeups = 0;
};

struct dummy_observer {
  template <class T>
  void on_next(const T&) {
    ++consumed;
  }

  void on_error(error what) {
    on_error_called = true;
    err = std::move(what);
  }

  void on_complete() {
    on_complete_called = true;
  }

  size_t consumed = 0;

  bool on_error_called = false;

  bool on_complete_called = false;

  error err;
};

} // namespace

BEGIN_FIXTURE_SCOPE(test_coordinator_fixture<>)

SCENARIO("SPSC buffers may go past their capacity") {
  GIVEN("an SPSC buffer with consumer and produer") {
    auto prod = make_counted<dummy_producer>();
    auto cons = make_counted<dummy_consumer>();
    auto buf = make_counted<async::spsc_buffer<int>>(10, 2);
    buf->set_producer(prod);
    buf->set_consumer(cons);
    CHECK_EQ(prod->consumer_ready, true);
    CHECK_EQ(prod->consumer_cancel, false);
    CHECK_EQ(prod->demand, 10u);
    CHECK_EQ(cons->producer_ready, true);
    CHECK_EQ(cons->producer_wakeups, 0u);
    WHEN("pushing into the buffer") {
      buf->push(1);
      CHECK_EQ(cons->producer_wakeups, 1u);
      buf->push(2);
      CHECK_EQ(cons->producer_wakeups, 1u);
      THEN("excess items are stored but do not trigger demand when consumed") {
        std::vector<int> tmp{3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
        buf->push(make_span(tmp));
        prod->demand = 0;
        CHECK_EQ(cons->producer_wakeups, 1u);
        MESSAGE("consume one element");
        {
          dummy_observer obs;
          auto [ok, consumed] = buf->pull(async::prioritize_errors, 1, obs);
          CHECK_EQ(ok, true);
          CHECK_EQ(consumed, 1u);
          CHECK_EQ(obs.consumed, 1u);
          CHECK_EQ(prod->demand, 0u);
        }
        MESSAGE("consume all remaining elements");
        {
          dummy_observer obs;
          auto [ok, consumed] = buf->pull(async::prioritize_errors, 20, obs);
          CHECK_EQ(ok, true);
          CHECK_EQ(consumed, 13u);
          CHECK_EQ(obs.consumed, 13u);
          CHECK_EQ(prod->demand, 10u);
        }
      }
    }
  }
}

SCENARIO("SPSC buffers moves data between actors") {
  GIVEN("a SPSC buffer resource") {
    WHEN("opening the resource from two actors") {
      THEN("data travels through the SPSC buffer") {
        using actor_t = event_based_actor;
        auto [rd, wr] = async::make_spsc_buffer_resource<int>(6, 2);
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
