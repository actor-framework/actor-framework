// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/async/promise.hpp"

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/scenario.hpp"

#include "caf/event_based_actor.hpp"
#include "caf/flow/scoped_coordinator.hpp"
#include "caf/scheduled_actor/flow.hpp"

using namespace caf;
using namespace std::literals;

namespace {

template <class T>
auto make_shared_val_ptr() {
  return std::make_shared<std::variant<none_t, T, error>>();
}

SCENARIO("futures can actively wait on a promise") {
  auto uut = async::promise<int32_t>{};
  auto fut = uut.get_future();
  GIVEN("a promise") {
    WHEN("future::get times out") {
      THEN("the client observes the error code sec::future_timeout") {
        check_eq(fut.get(1ms), make_error(sec::future_timeout));
      }
    }
    WHEN("future::get retrieves an error while waiting") {
      auto worker = std::thread{[&uut] {
        std::this_thread::sleep_for(5ms);
        uut.set_error(sec::runtime_error);
      }};
      THEN("the client observes the error code from set_error") {
        check_eq(fut.get(), make_error(sec::runtime_error));
      }
      worker.join();
    }
    WHEN("future::get with a timeout retrieves an error while waiting") {
      auto worker = std::thread{[&uut] {
        std::this_thread::sleep_for(5ms);
        uut.set_error(sec::runtime_error);
      }};
      THEN("the client observes the error code from set_error") {
        check_eq(fut.get(60s), make_error(sec::runtime_error));
      }
      worker.join();
    }
    WHEN("future::get retrieves a value while waiting") {
      auto worker = std::thread{[&uut] {
        std::this_thread::sleep_for(5ms);
        uut.set_value(42);
      }};
      THEN("the client observes the error code from set_error") {
        check_eq(fut.get(), int32_t{42});
      }
      worker.join();
    }
    WHEN("future::get with a timeout retrieves a value while waiting") {
      auto worker = std::thread{[&uut] {
        std::this_thread::sleep_for(5ms);
        uut.set_value(42);
      }};
      THEN("the client observes the error code from set_error") {
        check_eq(fut.get(60s), int32_t{42});
      }
      worker.join();
    }
  }
}

WITH_FIXTURE(test::fixture::deterministic) {

SCENARIO("actors can observe futures") {
  GIVEN("a promise and future pair") {
    WHEN("passing a non-ready future to an actor") {
      THEN("it can observe the value via .then() later") {
        auto val = make_shared_val_ptr<std::string>();
        auto uut = async::promise<std::string>{};
        auto fut = uut.get_future();
        auto testee = sys.spawn([val, fut](event_based_actor* self) {
          fut.bind_to(self).then([val](const std::string& str) { *val = str; },
                                 [val](const error& err) { *val = err; });
        });
        dispatch_messages();
        check(std::holds_alternative<none_t>(*val));
        uut.set_value("hello world"s);
        expect<action>().to(testee);
        if (check(std::holds_alternative<std::string>(*val)))
          check_eq(std::get<std::string>(*val), "hello world");
      }
      AND_THEN("it can observe the value via .observe_on() later") {
        auto val = make_shared_val_ptr<std::string>();
        auto uut = async::promise<std::string>{};
        auto fut = uut.get_future();
        auto testee = sys.spawn([val, fut](event_based_actor* self) {
          fut.observe_on(self).for_each(
            [val](const std::string& str) { *val = str; },
            [val](const error& err) { *val = err; });
        });
        dispatch_messages();
        check(std::holds_alternative<none_t>(*val));
        uut.set_value("hello world"s);
        expect<action>().to(testee);
        if (check(std::holds_alternative<std::string>(*val)))
          check_eq(std::get<std::string>(*val), "hello world");
      }
    }
    WHEN("passing a ready future to an actor") {
      THEN("it can observe the value via .then() immediately") {
        auto val = make_shared_val_ptr<std::string>();
        auto uut = async::promise<std::string>{};
        auto fut = uut.get_future();
        uut.set_value("hello world"s);
        auto testee = sys.spawn([val, fut](event_based_actor* self) {
          fut.bind_to(self).then([val](const std::string& str) { *val = str; },
                                 [val](const error& err) { *val = err; });
        });
        dispatch_messages();
        if (check(std::holds_alternative<std::string>(*val)))
          check_eq(std::get<std::string>(*val), "hello world");
      }
      AND_THEN("it can observe the value via .observe_on() immediately") {
        auto val = make_shared_val_ptr<std::string>();
        auto uut = async::promise<std::string>{};
        auto fut = uut.get_future();
        uut.set_value("hello world"s);
        auto testee = sys.spawn([val, fut](event_based_actor* self) {
          fut.observe_on(self).for_each(
            [val](const std::string& str) { *val = str; },
            [val](const error& err) { *val = err; });
        });
        dispatch_messages();
        if (check(std::holds_alternative<std::string>(*val)))
          check_eq(std::get<std::string>(*val), "hello world");
      }
    }
    WHEN("passing a non-ready future to an actor and disposing the action") {
      THEN("it never observes the value with .then()") {
        auto val = make_shared_val_ptr<std::string>();
        auto uut = async::promise<std::string>{};
        auto fut = uut.get_future();
        auto hdl = disposable{};
        auto testee = sys.spawn([val, fut, &hdl](event_based_actor* self) {
          hdl = fut.bind_to(self).then(
            [val](const std::string& str) { *val = str; },
            [val](const error& err) { *val = err; });
        });
        dispatch_messages();
        check(std::holds_alternative<none_t>(*val));
        hdl.dispose();
        uut.set_value("hello world"s);
        dispatch_messages();
        check(std::holds_alternative<none_t>(*val));
      }
    }
  }
}

SCENARIO("never setting a value or an error breaks the promises") {
  GIVEN("multiple promises that point to the same cell") {
    WHEN("the last promise goes out of scope") {
      THEN("the future reports a broken promise when using .then()") {
        using promise_t = async::promise<int32_t>;
        using future_t = async::future<int32_t>;
        future_t fut;
        {
          auto uut = promise_t{};
          fut = uut.get_future();
          check(fut.pending());
          {
            // copy ctor
            promise_t cpy{uut};
            check(fut.pending());
            // move ctor
            promise_t mv{std::move(cpy)};
            check(fut.pending());
            {
              // copy assign
              promise_t cpy2;
              cpy2 = mv;
              check(fut.pending());
              // move assign
              promise_t mv2;
              mv2 = std::move(mv);
              check(fut.pending());
            }
            check(fut.pending());
          }
          check(fut.pending());
        }
        check(!fut.pending());
        auto ctx = flow::scoped_coordinator::make();
        size_t observed_events = 0;
        fut.bind_to(ctx.get()).then(
          [this, &observed_events](int32_t) {
            ++observed_events;
            fail("unexpected value");
          },
          [this, &observed_events](const error& err) {
            ++observed_events;
            check_eq(err, make_error(sec::broken_promise));
          });
        ctx->run();
        check_eq(observed_events, 1u);
      }
      AND_THEN("the future reports a broken promise when using .observe_on()") {
        using promise_t = async::promise<int32_t>;
        using future_t = async::future<int32_t>;
        future_t fut;
        {
          auto uut = promise_t{};
          fut = uut.get_future();
          check(fut.pending());
        }
        check(!fut.pending());
        auto val = make_shared_val_ptr<int32_t>();
        sys.spawn([val, fut](event_based_actor* self) {
          fut.observe_on(self).for_each([val](int32_t i) { *val = i; },
                                        [val](const error& err) {
                                          *val = err;
                                        });
        });
        dispatch_messages();
        if (check(std::holds_alternative<error>(*val)))
          check_eq(std::get<error>(*val), sec::broken_promise);
      }
    }
  }
}

} // WITH_FIXTURE(test::fixture::deterministic)

} // namespace
