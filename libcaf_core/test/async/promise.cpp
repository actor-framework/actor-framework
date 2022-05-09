// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE async.promise

#include "caf/async/promise.hpp"

#include "core-test.hpp"

#include "caf/flow/scoped_coordinator.hpp"
#include "caf/scheduled_actor/flow.hpp"

using namespace caf;
using namespace std::literals;

BEGIN_FIXTURE_SCOPE(test_coordinator_fixture<>)

SCENARIO("actors can observe futures") {
  GIVEN("a promise and future pair") {
    WHEN("passing a non-ready future to an actor") {
      THEN("the actor can observe the value via .then() later") {
        auto val = std::make_shared<std::variant<none_t, std::string, error>>();
        auto uut = async::promise<std::string>{};
        auto fut = uut.get_future();
        auto testee = sys.spawn([val, fut](event_based_actor* self) {
          fut.bind_to(self).then([val](const std::string& str) { *val = str; },
                                 [val](const error& err) { *val = err; });
        });
        run();
        CHECK(std::holds_alternative<none_t>(*val));
        uut.set_value("hello world"s);
        expect((action), to(testee));
        if (CHECK(std::holds_alternative<std::string>(*val)))
          CHECK_EQ(std::get<std::string>(*val), "hello world");
      }
    }
    WHEN("passing a ready future to an actor") {
      THEN("the actor can observe the value via .then() immediately") {
        auto val = std::make_shared<std::variant<none_t, std::string, error>>();
        auto uut = async::promise<std::string>{};
        auto fut = uut.get_future();
        uut.set_value("hello world"s);
        auto testee = sys.spawn([val, fut](event_based_actor* self) {
          fut.bind_to(self).then([val](const std::string& str) { *val = str; },
                                 [val](const error& err) { *val = err; });
        });
        run();
        if (CHECK(std::holds_alternative<std::string>(*val)))
          CHECK_EQ(std::get<std::string>(*val), "hello world");
      }
    }
    WHEN("passing a non-ready future to an actor and disposing the action") {
      THEN("the actor never observes the value") {
        auto val = std::make_shared<std::variant<none_t, std::string, error>>();
        auto uut = async::promise<std::string>{};
        auto fut = uut.get_future();
        auto hdl = disposable{};
        auto testee = sys.spawn([val, fut, &hdl](event_based_actor* self) {
          hdl = fut.bind_to(self).then(
            [val](const std::string& str) { *val = str; },
            [val](const error& err) { *val = err; });
        });
        run();
        CHECK(std::holds_alternative<none_t>(*val));
        hdl.dispose();
        uut.set_value("hello world"s);
        run();
        CHECK(std::holds_alternative<none_t>(*val));
      }
    }
  }
}

SCENARIO("never setting a value or an error breaks the promises") {
  GIVEN("multiple promises that point to the same cell") {
    WHEN("the last promise goes out of scope") {
      THEN("the future reports a broken promise") {
        using promise_t = async::promise<int32_t>;
        using future_t = async::future<int32_t>;
        future_t fut;
        {
          auto uut = promise_t{};
          fut = uut.get_future();
          CHECK(fut.pending());
          {
            // copy ctor
            promise_t cpy{uut};
            CHECK(fut.pending());
            // move ctor
            promise_t mv{std::move(cpy)};
            CHECK(fut.pending());
            {
              // copy assign
              promise_t cpy2;
              cpy2 = mv;
              CHECK(fut.pending());
              // move assign
              promise_t mv2;
              mv2 = std::move(mv);
              CHECK(fut.pending());
            }
            CHECK(fut.pending());
          }
          CHECK(fut.pending());
        }
        CHECK(!fut.pending());
        auto ctx = flow::scoped_coordinator::make();
        size_t observed_events = 0;
        fut.bind_to(ctx.get()).then(
          [&observed_events](int32_t) {
            ++observed_events;
            FAIL("unexpected value");
          },
          [&observed_events](const error& err) {
            ++observed_events;
            CHECK_EQ(err, make_error(sec::broken_promise));
          });
        ctx->run();
        CHECK_EQ(observed_events, 1u);
      }
    }
  }
}

END_FIXTURE_SCOPE()
