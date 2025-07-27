// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/async/spsc_buffer.hpp"

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/async/mock_consumer.test.hpp"
#include "caf/async/mock_producer.test.hpp"

#include "caf/actor_from_state.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/exit_reason.hpp"
#include "caf/flow/coordinator.hpp"
#include "caf/flow/observable_builder.hpp"
#include "caf/flow/observer.hpp"
#include "caf/log/test.hpp"
#include "caf/scheduled_actor/flow.hpp"
#include "caf/scoped_actor.hpp"

#include <memory>

using namespace caf;
using namespace std::literals;

namespace {

struct mock_observer {
  mock_observer(std::vector<int>& items) : items(&items) {
    // nop
  }

  void on_next(const int& item) {
    items->emplace_back(item);
  }

  void on_complete() {
    completed = true;
  }

  void on_error(const error&) {
    failed = true;
  }

  std::vector<int>* items;

  bool completed = false;

  bool failed = false;
};

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

WITH_FIXTURE(test::fixture::deterministic) {

TEST("resources may be copied") {
  auto [rd, wr] = async::make_spsc_buffer_resource<int>(6, 2);
  // Test copy constructor.
  async::consumer_resource<int> rd2{rd};
  check_eq(rd, rd2);
  async::producer_resource<int> wr2{wr};
  check_eq(wr, wr2);
  // Test copy-assignment.
  async::consumer_resource<int> rd3;
  check_ne(rd2, rd3);
  rd3 = rd2;
  check_eq(rd2, rd3);
  async::producer_resource<int> wr3;
  check_ne(wr2, wr3);
  wr3 = wr2;
  check_eq(wr2, wr3);
}

TEST("resources may be moved") {
  auto [rd, wr] = async::make_spsc_buffer_resource<int>(6, 2);
  check(static_cast<bool>(rd));
  check(static_cast<bool>(wr));
  // Test move constructor.
  async::consumer_resource<int> rd2{std::move(rd)};
  check(!rd); // NOLINT(bugprone-use-after-move)
  check(rd2.valid());
  async::producer_resource<int> wr2{std::move(wr)};
  check(!wr); // NOLINT(bugprone-use-after-move)
  check(wr2.valid());
  // Test move-assignment.
  async::consumer_resource<int> rd3{std::move(rd2)};
  check(!rd2); // NOLINT(bugprone-use-after-move)
  check(rd3.valid());
  async::producer_resource<int> wr3{std::move(wr2)};
  check(!wr2); // NOLINT(bugprone-use-after-move)
  check(wr3.valid());
}

SCENARIO("SPSC buffers may go past their capacity") {
  GIVEN("an SPSC buffer with consumer and produer") {
    auto prod = make_counted<dummy_producer>();
    auto cons = make_counted<dummy_consumer>();
    auto buf = make_counted<async::spsc_buffer<int>>(10, 2);
    buf->set_producer(prod);
    buf->set_consumer(cons);
    check_eq(prod->consumer_ready, true);
    check_eq(prod->consumer_cancel, false);
    check_eq(prod->demand, 10u);
    check_eq(cons->producer_ready, true);
    check_eq(cons->producer_wakeups, 0u);
    WHEN("pushing into the buffer") {
      buf->push(1);
      check_eq(cons->producer_wakeups, 1u);
      buf->push(2);
      check_eq(cons->producer_wakeups, 1u);
      THEN("excess items are stored but do not trigger demand when consumed") {
        auto tmp = std::vector<int>{3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
        buf->push(make_span(tmp));
        prod->demand = 0;
        check_eq(cons->producer_wakeups, 1u);
      }
      AND_THEN("pull(1) consumes one element") {
        dummy_observer obs;
        auto [ok, consumed] = buf->pull(async::prioritize_errors, 1, obs);
        check_eq(ok, true);
        check_eq(consumed, 1u);
        check_eq(obs.consumed, 1u);
        check_eq(prod->demand, 0u);
      }
      AND_THEN("pull(20) consumes all remaining elements") {
        dummy_observer obs;
        auto [ok, consumed] = buf->pull(async::prioritize_errors, 20, obs);
        check_eq(ok, true);
        check_eq(consumed, 13u);
        check_eq(obs.consumed, 13u);
        check_eq(prod->demand, 10u);
      }
    }
  }
}

SCENARIO("the prioritize_errors policy skips processing of pending items") {
  GIVEN("an SPSC buffer with consumer and produer") {
    auto prod = make_counted<dummy_producer>();
    auto cons = make_counted<dummy_consumer>();
    auto buf = make_counted<async::spsc_buffer<int>>(10, 2);
    auto tmp = std::vector<int>{1, 2, 3, 4, 5};
    WHEN("pushing into the buffer and then aborting") {
      THEN("pulling items with prioritize_errors skips remaining items") {
        buf->set_producer(prod);
        buf->push(make_span(tmp));
        buf->set_consumer(cons);
        check_eq(cons->producer_wakeups, 1u);
      }
      AND_THEN("pull(1) consumes one element") {
        dummy_observer obs;
        auto [ok, consumed] = buf->pull(async::prioritize_errors, 1, obs);
        check_eq(ok, true);
        check_eq(consumed, 1u);
        check_eq(obs.consumed, 1u);
      }
      AND_THEN("calling abort will cause the next pull(1) to return an error") {
        buf->abort(sec::runtime_error);
        dummy_observer obs;
        auto [ok, consumed] = buf->pull(async::prioritize_errors, 1, obs);
        check_eq(ok, false);
        check_eq(consumed, 0u);
        check_eq(obs.err, sec::runtime_error);
      }
    }
  }
}

SCENARIO("SPSC buffers moves data between actors") {
  GIVEN("an SPSC buffer resource") {
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
        dispatch_messages();
        check_eq(inputs, outputs);
      }
    }
  }
}

SCENARIO("SPSC buffers appear empty when only one actor is connected") {
  GIVEN("an SPSC buffer resource") {
    WHEN("destroying the write end before adding a subscriber") {
      THEN("no data arrives through the SPSC buffer") {
        using actor_t = event_based_actor;
        auto outputs = std::vector<int>{};
        auto finalized = false;
        {
          auto [rd, wr] = async::make_spsc_buffer_resource<int>(6, 2);
          sys.spawn([rd{rd}, &outputs, &finalized](actor_t* snk) {
            snk
              ->make_observable() //
              .from_resource(rd)
              .do_finally([&finalized] { finalized = true; })
              .for_each([&outputs](int x) { outputs.emplace_back(x); });
          });
        }
        // At scope exit, `wr` gets destroyed, closing the buffer.
        dispatch_messages();
        check(finalized);
        check(outputs.empty());
      }
    }
    WHEN("destroying the write end after adding a subscriber") {
      THEN("no data arrives through the SPSC buffer") {
        using actor_t = event_based_actor;
        auto outputs = std::vector<int>{};
        auto finalized = false;
        {
          auto [rd, wr] = async::make_spsc_buffer_resource<int>(6, 2);
          sys.spawn([rd{rd}, &outputs, &finalized](actor_t* snk) {
            snk
              ->make_observable() //
              .from_resource(rd)
              .do_finally([&finalized] { finalized = true; })
              .for_each([&outputs](int x) { outputs.emplace_back(x); });
          });
          // Only difference to before: have the actor create the observable
          // from `rd` handle before destroying `wr`.
          dispatch_messages();
        }
        // At scope exit, `wr` gets destroyed, closing the buffer.
        dispatch_messages();
        check(finalized);
        check(outputs.empty());
      }
    }
    WHEN("aborting the write end") {
      THEN("the observer receives on_error") {
        using actor_t = event_based_actor;
        auto outputs = std::vector<int>{};
        auto on_error_called = false;
        auto [rd, wr] = async::make_spsc_buffer_resource<int>(6, 2);
        sys.spawn([this, rd{rd}, &outputs, &on_error_called](actor_t* snk) {
          snk
            ->make_observable() //
            .from_resource(rd)
            .do_on_error([this, &on_error_called](const error& err) {
              on_error_called = true;
              check_eq(err, sec::runtime_error);
            })
            .for_each([&outputs](int x) { outputs.emplace_back(x); });
        });
        wr.abort(sec::runtime_error);
        wr.abort(sec::runtime_error); // Calling twice must have no side effect.
        dispatch_messages();
        check(on_error_called);
        check(outputs.empty());
      }
    }
    WHEN("closing the write end") {
      THEN("the observer receives on_complete") {
        using actor_t = event_based_actor;
        auto outputs = std::vector<int>{};
        auto on_complete_called = false;
        auto [rd, wr] = async::make_spsc_buffer_resource<int>(6, 2);
        sys.spawn([rd{rd}, &outputs, &on_complete_called](actor_t* snk) {
          snk
            ->make_observable() //
            .from_resource(rd)
            .do_on_complete([&on_complete_called] { //
              on_complete_called = true;
            })
            .for_each([&outputs](int x) { outputs.emplace_back(x); });
        });
        wr.close();
        wr.close(); // Calling twice must have no side effect.
        dispatch_messages();
        check(on_complete_called);
        check(outputs.empty());
      }
    }
  }
}

SCENARIO("SPSC buffers drop data when discarding the read end") {
  GIVEN("an SPSC buffer resource") {
    WHEN("destroying the read end before adding a publisher") {
      THEN("the flow of the writing actor gets canceled") {
        using actor_t = event_based_actor;
        auto outputs = std::vector<int>{};
        {
          auto [rd, wr] = async::make_spsc_buffer_resource<int>(6, 2);
          sys.spawn([wr{wr}](actor_t* src) {
            src
              ->make_observable() //
              .iota(1)
              .subscribe(wr);
          });
        }
        // At scope exit, `rd` gets destroyed, closing the buffer.
        dispatch_messages();
        check(outputs.empty());
      }
    }
    WHEN("destroying the read end before adding a publisher") {
      THEN("the flow of the writing actor gets canceled") {
        using actor_t = event_based_actor;
        auto outputs = std::vector<int>{};
        {
          auto [rd, wr] = async::make_spsc_buffer_resource<int>(6, 2);
          sys.spawn([wr{wr}](actor_t* src) {
            src
              ->make_observable() //
              .iota(1)
              .subscribe(wr);
          });
          // Only difference to before: have the actor add an observer that
          // writes to `wr` before destroying `rd`.
          dispatch_messages();
        }
        // At scope exit, `rd` gets destroyed, closing the buffer.
        dispatch_messages();
        check(outputs.empty());
      }
    }
    WHEN("canceling the read end before adding a publisher") {
      THEN("the flow of the writing actor gets canceled") {
        using actor_t = event_based_actor;
        auto outputs = std::vector<int>{};
        auto [rd, wr] = async::make_spsc_buffer_resource<int>(6, 2);
        sys.spawn([wr{wr}](actor_t* src) {
          src
            ->make_observable() //
            .iota(1)
            .subscribe(wr);
        });
        rd.cancel();
        rd.cancel(); // Calling twice must have no side effect.
        dispatch_messages();
        check(outputs.empty());
      }
    }
  }
}

SCENARIO("resources are invalid after calling try_open") {
  GIVEN("a producer resource") {
    WHEN("opening it twice") {
      THEN("the second try_open fails") {
        auto [rd, wr] = async::make_spsc_buffer_resource<int>(6, 2);
        check(rd.valid());
        check_ne(rd.try_open(), nullptr);
        check(!rd);
        check_eq(rd.try_open(), nullptr);
      }
    }
  }
}

SCENARIO("producer resources may be subscribed to flows only once") {
  GIVEN("a producer resource") {
    WHEN("subscribing it to a flow twice") {
      THEN("the second attempt results in a canceled subscription") {
        using actor_t = event_based_actor;
        auto outputs = std::vector<int>{};
        auto [rd, wr] = async::make_spsc_buffer_resource<int>(6, 2);
        auto prod1 = sys.spawn([wr{wr}](actor_t* src) {
          src
            ->make_observable() //
            .iota(1)
            .subscribe(wr);
        });
        auto prod2 = sys.spawn([wr{wr}](actor_t* src) {
          src
            ->make_observable() //
            .iota(1)
            .subscribe(wr);
        });
        dispatch_messages();
        check(!terminated(prod1));
        check(terminated(prod2));
        rd.cancel();
        dispatch_messages();
        check(terminated(prod1));
        check(terminated(prod2));
      }
    }
  }
}

SCENARIO("consumer resources may be converted to flows only once") {
  GIVEN("a consumer resource") {
    WHEN("making an observable from the resource") {
      THEN("the second attempt results in an empty observable") {
        using actor_t = event_based_actor;
        auto outputs = std::vector<int>{};
        auto [rd, wr] = async::make_spsc_buffer_resource<int>(6, 2);
        auto snk1 = sys.spawn([rd{rd}, &outputs](actor_t* snk) {
          snk
            ->make_observable() //
            .from_resource(rd)
            .for_each([&outputs](int x) { outputs.emplace_back(x); });
        });
        auto snk2 = sys.spawn([rd{rd}, &outputs](actor_t* snk) {
          snk
            ->make_observable() //
            .from_resource(rd)
            .for_each([&outputs](int x) { outputs.emplace_back(x); });
        });
        check(!terminated(snk1));
        check(terminated(snk2));
        wr.close();
        dispatch_messages();
        check(terminated(snk1));
        check(terminated(snk2));
      }
    }
  }
}

SCENARIO("actors can consume items from SPSC buffers directly") {
  GIVEN("a consumer resource") {
    auto [rd, wr] = async::make_spsc_buffer_resource<int>(6, 2);
    WHEN("binding it to an actor") {
      auto wakeups = std::make_shared<int>(0);
      auto items = std::make_shared<std::vector<int>>();
      auto snk = sys.spawn(
        [this, rd = rd, wakeups, items](event_based_actor* self) mutable {
          auto ptr = rd.consume_on(self, [wakeups, items](auto& src) {
            ++*wakeups;
            mock_observer obs{*items};
            auto [again, pulled] = src.pull(100, obs);
            caf::log::test::debug("again: {}, pulled: {}", again, pulled);
            caf::log::test::debug("completed: {}, failed: {}", obs.completed,
                                  obs.failed);
          });
          check_ne(ptr, nullptr);
        });
      THEN("the actor receives the items from the buffer") {
        auto buf = wr.try_open();
        require_ne(buf, nullptr);
        auto prod = make_counted<async::mock_producer>();
        buf->set_producer(prod);
        auto buf_guard = detail::scope_guard([buf]() noexcept {
          try {
            buf->close();
          } catch (...) {
            fprintf(stderr, "failed to close buffer! %s:%d\n", __FILE__,
                    __LINE__);
          }
        });
        check_eq(*wakeups, 0);
        require_ne(buf, nullptr);
        check_gt(buf->push(1), 0u);
        check_gt(buf->push(2), 0u);
        expect<action>().to(snk);
        if (check_eq(items->size(), 2u)) {
          check_eq(items->at(0), 1);
          check_eq(items->at(1), 2);
        }
        check_eq(*wakeups, 1);
        check_eq(mail_count(snk), 0u);
        buf->close();
        expect<action>().to(snk);
        check_eq(*wakeups, 2);
        check_eq(snk->ctrl()->strong_refs.load(), 1u);
      }
    }
  }
}

SCENARIO("actors can dispose buffer consumers") {
  GIVEN("a consumer resource") {
    auto [rd, wr] = async::make_spsc_buffer_resource<int>(6, 2);
    WHEN("binding it to an actor") {
      auto wakeups = std::make_shared<int>(0);
      auto items = std::make_shared<std::vector<int>>();
      auto snk
        = sys.spawn([rd{rd}, wakeups, items](event_based_actor* snk) mutable {
            std::ignore = rd.consume_on(snk, [wakeups, items](auto& src) {
              ++*wakeups;
              mock_observer obs{*items};
              auto [again, pulled] = src.pull(1, obs);
              caf::log::test::debug("again: {}, pulled: {}", again, pulled);
              caf::log::test::debug("completed: {}, failed: {}", obs.completed,
                                    obs.failed);
              while (again && pulled > 0 && items->size() < 2) {
                std::tie(again, pulled) = src.pull(1, obs);
              }
              if (items->size() == 2) {
                src.dispose();
              }
            });
          });
      THEN("the actor receives the items from the buffer") {
        check_eq(*wakeups, 0);
        auto buf = wr.try_open();
        require_ne(buf, nullptr);
        auto prod = make_counted<async::mock_producer>();
        buf->set_producer(prod);
        auto buf_guard = detail::scope_guard([buf]() noexcept {
          try {
            buf->close();
          } catch (...) {
            fprintf(stderr, "failed to close buffer! %s:%d\n", __FILE__,
                    __LINE__);
          }
        });
        require_ne(buf, nullptr);
        check_gt(buf->push(1), 0u);
        check_gt(buf->push(2), 0u);
        check_gt(buf->push(3), 0u);
        expect<action>().to(snk);
        check(prod->canceled.load());
        if (check_eq(items->size(), 2u)) {
          check_eq(items->at(0), 1);
          check_eq(items->at(1), 2);
        }
        check_eq(mail_count(snk), 0u);
        check_eq(snk->ctrl()->strong_refs.load(), 1u);
      }
    }
  }
}

SCENARIO("actors can produce items to SPSC buffers directly") {
  GIVEN("a producer resource") {
    auto [rd, wr] = async::make_spsc_buffer_resource<int>(6, 2);
    WHEN("binding it to an actor") {
      auto demand = std::make_shared<size_t>(0);
      auto canceled = std::make_shared<bool>(false);
      auto src = sys.spawn(
        [this, wr = wr, demand, canceled](event_based_actor* self) mutable {
          auto ptr = wr.produce_on(
            self,
            [demand, first = false](auto& out, size_t new_demand) mutable {
              *demand += new_demand;
              if (first) {
                out.push(1);
                first = false;
              }
            },
            [canceled](auto&) { *canceled = true; });
          check_ne(ptr, nullptr);
        });
      THEN("the actor receives the items from the buffer") {
        auto buf = rd.try_open();
        require_ne(buf, nullptr);
        auto con = make_counted<async::mock_consumer>();
        auto buf_guard = detail::scope_guard([buf]() noexcept {
          try {
            buf->cancel();
          } catch (...) {
            fprintf(stderr, "failed to cancel buffer subscription! %s:%d\n",
                    __FILE__, __LINE__);
          }
        });
        check_eq(mail_count(), 0u);
        buf->set_consumer(con);
        expect<action>().to(src);
        check_eq(*demand, 6u); // Initial demand = capacity.
        check_eq(mail_count(), 0u);
        check_eq(con->wakeups.load(), 1u);
        buf->cancel();
        expect<action>().to(src);
        check(*canceled);
        check_eq(src->ctrl()->strong_refs.load(), 1u);
      }
    }
  }
}

#ifdef CAF_ENABLE_EXCEPTIONS

// Note: this basically checks that buffer protects against misuse and is not
//       how users should do things.
SCENARIO("SPSC buffers reject multiple producers") {
  GIVEN("an SPSC buffer resource") {
    WHEN("attaching a second producer") {
      THEN("the buffer immediately calls on_consumer_cancel on it") {
        // Note: two resources should never point to the same buffer.
        using actor_t = event_based_actor;
        using buffer_type = async::spsc_buffer<int>;
        auto buf = make_counted<buffer_type>(20, 5);
        auto rd = async::consumer_resource<int>{buf};
        auto wr1 = async::producer_resource<int>{buf};
        auto wr2 = async::producer_resource<int>{buf};
        auto prod1 = sys.spawn([wr1](actor_t* src) {
          src->make_observable().iota(1).subscribe(wr1);
        });
        auto prod2 = sys.spawn([wr2](actor_t* src) {
          src->make_observable().iota(1).subscribe(wr2);
        });
        dispatch_messages();
        check(!terminated(prod1));
        check(terminated(prod2));
        rd.cancel();
        dispatch_messages();
        check(terminated(prod1));
        check(terminated(prod2));
      }
    }
  }
}

// Note: this basically checks that buffer protects against misuse and is not
//       how users should do things.
SCENARIO("SPSC buffers reject multiple consumers") {
  GIVEN("an SPSC buffer resource") {
    WHEN("attaching a second consumer") {
      THEN("the buffer throws an exception") {
        // Note: two resources should never point to the same buffer.
        using actor_t = event_based_actor;
        using buffer_type = async::spsc_buffer<int>;
        auto buf = make_counted<buffer_type>(20, 5);
        auto rd1 = async::consumer_resource<int>{buf};
        auto rd2 = async::consumer_resource<int>{buf};
        auto wr = async::producer_resource<int>{buf};
        auto outputs = std::vector<int>{};
        auto snk1 = sys.spawn([rd1, &outputs](actor_t* snk) {
          snk
            ->make_observable() //
            .from_resource(rd1)
            .for_each([&outputs](int x) { outputs.emplace_back(x); });
        });
        auto snk2 = sys.spawn([rd2, &outputs](actor_t* snk) {
          snk
            ->make_observable() //
            .from_resource(rd2)
            .for_each([&outputs](int x) { outputs.emplace_back(x); });
        });
        dispatch_messages();
        check(!terminated(snk1));
        check(terminated(snk2));
        wr.close();
        dispatch_messages();
        check(terminated(snk1));
        check(terminated(snk2));
      }
    }
  }
}

#endif // CAF_ENABLE_EXCEPTIONS

} // WITH_FIXTURE(test::fixture::deterministic)

namespace {

class buffer_multiplexer_state {
public:
  using consumer_resource = async::consumer_resource<int>;

  using consumer_ptr = async::spsc_buffer_consumer_ptr<int>;

  using producer_resource = async::producer_resource<int>;

  using producer_ptr = async::spsc_buffer_producer_ptr<int>;

  static constexpr const char* name = "buffer_multiplexer";

  buffer_multiplexer_state(caf::event_based_actor* self, consumer_resource rd1,
                           consumer_resource rd2, consumer_resource rd3,
                           producer_resource wr4)
    : self(self) {
    add_source(1, rd1);
    add_source(2, rd2);
    add_source(3, rd3);
    dst = wr4.produce_on(
      self,
      [this](auto&, size_t new_demand) { // on demand
        demand += new_demand;
        run();
      },
      [this, self](auto&) {
        for (auto& [id, src] : sources) { // on cancel
          src->dispose();
        }
        sources.clear();
        ready_sources.clear();
        self->quit();
      });
  }

  behavior make_behavior() {
    return {};
  }

  void add_source(int id, consumer_resource rd) {
    auto src = rd.consume_on(self, [this, id](auto&) {
      ready_sources.push_back(id);
      run();
    });
    if (!src) {
      fprintf(stderr, "failed to consume on source %d\n", id);
      abort();
    }
    sources.emplace(id, src);
  }

  void run() {
    while (demand > 0 && !ready_sources.empty()) {
      auto id = ready_sources.front();
      ready_sources.pop_front();
      if (auto i = sources.find(id); i != sources.end()) {
        mock_observer obs{buffer};
        auto [again, pulled] = i->second->pull(demand, obs);
        demand -= pulled;
        if (pulled != 0) {
          dst->push(buffer);
          buffer.clear();
        }
        if (!again) {
          sources.erase(i);
          if (sources.empty()) {
            dst->dispose();
            self->quit();
            return;
          }
        } else if (pulled != 0) {
          ready_sources.push_back(id);
        }
      }
    }
  }

  caf::event_based_actor* self;

  size_t demand = 0;

  std::map<int, consumer_ptr> sources;

  std::deque<int> ready_sources;

  producer_ptr dst;

  std::vector<int> buffer;
};

} // namespace

TEST("actors can multiplex SPSC buffers") {
  // Non-deterministic test that has one actor reading from multiple SPSC
  // buffers and producing to a single buffer.
  auto [rd1, wr1] = async::make_spsc_buffer_resource<int>(50, 10);
  auto [rd2, wr2] = async::make_spsc_buffer_resource<int>(50, 10);
  auto [rd3, wr3] = async::make_spsc_buffer_resource<int>(50, 10);
  auto [rd4, wr4] = async::make_spsc_buffer_resource<int>(50, 10);
  caf::actor_system_config cfg;
  caf::actor_system sys{cfg};
  // Spin up three actors, producing 1000 items each.
  auto src1 = sys.spawn([wr = wr1](event_based_actor* self) {
    self->make_observable().iota(1).take(1000).subscribe(wr);
  });
  auto src2 = sys.spawn([wr = wr2](event_based_actor* self) {
    self->make_observable().iota(1001).take(1000).subscribe(wr);
  });
  auto src3 = sys.spawn([wr = wr3](event_based_actor* self) {
    self->make_observable().iota(2001).take(1000).subscribe(wr);
  });
  // Spin up the consumer that collects all items into a vector.
  auto items = std::make_shared<std::vector<int>>();
  items->reserve(3000);
  auto snk = sys.spawn([rd = rd4, items](event_based_actor* self) mutable {
    std::ignore = rd.consume_on(self, [self, items](auto& src) {
      mock_observer obs{*items};
      auto [again, pulled] = src.pull(100, obs);
      while (again && pulled > 0) {
        std::tie(again, pulled) = src.pull(100, obs);
      }
      if (!again) {
        self->quit();
      }
    });
  });
  // Spin up the multiplexer that reads rd1, rd2, and rd3 and pushes to wr4.
  auto buffer_multiplexer = actor_from_state<buffer_multiplexer_state>;
  auto mpx = sys.spawn(buffer_multiplexer, rd1, rd2, rd3, wr4);
  // Wait up to 3s for the test to finish.
  caf::scoped_actor self{sys};
  auto ok = false;
  self->monitor(snk);
  self->receive([&ok](const caf::down_msg&) { ok = true; }, after(3s) >> [] {});
  if (check(ok)) {
    std::sort(items->begin(), items->end());
    items->erase(std::unique(items->begin(), items->end()), items->end());
    if (check_eq(items->size(), 3000u)) {
      check_eq(items->front(), 1);
      check_eq(items->back(), 3000);
    }
  } else {
    anon_send_exit(src1, exit_reason::kill);
    anon_send_exit(src2, exit_reason::kill);
    anon_send_exit(src3, exit_reason::kill);
    anon_send_exit(snk, exit_reason::kill);
    anon_send_exit(mpx, exit_reason::kill);
  }
}
