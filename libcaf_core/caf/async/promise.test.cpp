// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/async/promise.hpp"

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/scenario.hpp"

#include "caf/error_code.hpp"
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

} // namespace

SCENARIO("futures can actively wait on a promise") {
  auto uut = async::promise<int32_t>{};
  auto fut = uut.get_future();
  GIVEN("a promise") {
    WHEN("future::get times out") {
      THEN("the client observes the error code sec::future_timeout") {
        check_eq(fut.get(1ms), error_code{sec::future_timeout});
      }
    }
    WHEN("future::get retrieves an error while waiting") {
      auto worker = std::thread{[&uut] {
        std::this_thread::sleep_for(5ms);
        uut.set_error(sec::runtime_error);
      }};
      THEN("the client observes the error code from set_error") {
        check_eq(fut.get(), error_code{sec::runtime_error});
      }
      worker.join();
    }
    WHEN("future::get with a timeout retrieves an error while waiting") {
      auto worker = std::thread{[&uut] {
        std::this_thread::sleep_for(5ms);
        uut.set_error(sec::runtime_error);
      }};
      THEN("the client observes the error code from set_error") {
        check_eq(fut.get(60s), error_code{sec::runtime_error});
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
    WHEN("disposing a non-ready future") {
      THEN("actors observe sec::disposed via .then()") {
        auto val = make_shared_val_ptr<std::string>();
        auto uut = async::promise<std::string>{};
        auto fut = uut.get_future();
        auto testee = sys.spawn([val, fut](event_based_actor* self) {
          fut.bind_to(self).then([val](const std::string& str) { *val = str; },
                                 [val](const error& err) { *val = err; });
        });
        dispatch_messages();
        check(std::holds_alternative<none_t>(*val));
        fut.dispose();
        expect<action>().to(testee);
        if (check(std::holds_alternative<error>(*val)))
          check_eq(std::get<error>(*val), sec::disposed);
      }
      AND_THEN("actors observe sec::disposed via .observe_on()") {
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
        fut.dispose();
        expect<action>().to(testee);
        if (check(std::holds_alternative<error>(*val)))
          check_eq(std::get<error>(*val), sec::disposed);
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

SCENARIO("futures can dispose a pending promise") {
  GIVEN("a pending promise and future pair") {
    auto uut = async::promise<int32_t>{};
    auto fut = uut.get_future();
    WHEN("calling future::dispose") {
      THEN("the call succeeds and the future observes sec::disposed") {
        check(fut.pending());
        fut.dispose();
        check(!fut.pending());
        check_eq(fut.get(), error_code{sec::disposed});
      }
    }
    WHEN("calling future::dispose twice") {
      THEN("the second call has no effect") {
        fut.dispose();
        check_eq(fut.get(), error_code{sec::disposed});
        fut.dispose();
        check_eq(fut.get(), error_code{sec::disposed});
      }
    }
    WHEN("multiple futures point to the same cell") {
      auto fut2 = uut.get_future();
      THEN("disposing one future disposes the result for all of them") {
        fut.dispose();
        check_eq(fut.get(), error_code{sec::disposed});
        check_eq(fut2.get(), error_code{sec::disposed});
      }
    }
    WHEN("the promise already has a value") {
      uut.set_value(42);
      THEN("future::dispose has no effect and leaves the value intact") {
        fut.dispose();
        check_eq(fut.get(), int32_t{42});
      }
    }
    WHEN("the promise already has an error") {
      uut.set_error(sec::runtime_error);
      THEN("future::dispose has no effect and leaves the error intact") {
        fut.dispose();
        check_eq(fut.get(), error_code{sec::runtime_error});
      }
    }
    WHEN("the future disposes before the promise sets a value") {
      THEN("the result remains disposed") {
        fut.dispose();
        uut.set_value(42);
        check_eq(fut.get(), error_code{sec::disposed});
      }
    }
    WHEN("the future disposes before the promise sets an error") {
      THEN("the result remains disposed") {
        fut.dispose();
        uut.set_error(sec::runtime_error);
        check_eq(fut.get(), error_code{sec::disposed});
      }
    }
  }
}

SCENARIO("promises can register on-dispose callbacks") {
  GIVEN("a pending promise") {
    auto uut = async::promise<int32_t>{};
    WHEN("registering a synchronous on-dispose callback") {
      auto disposed = std::make_shared<bool>(false);
      check(uut.set_on_dispose(nullptr, make_single_shot_action(
                                          [disposed] { *disposed = true; })));
      auto fut = uut.get_future();
      THEN("dispose runs the callback") {
        fut.dispose();
        check(*disposed);
      }
    }
    WHEN("registering an on-dispose callback on an execution context") {
      auto disposed = std::make_shared<bool>(false);
      auto ctx = flow::scoped_coordinator::make();
      check(uut.set_on_dispose(ctx.get(), make_single_shot_action(
                                            [disposed] { *disposed = true; })));
      auto fut = uut.get_future();
      THEN("dispose schedules the callback on that context") {
        fut.dispose();
        check(!*disposed);
        ctx->run();
        check(*disposed);
      }
    }
    WHEN("the promise already has a value") {
      uut.set_value(42);
      THEN("set_on_dispose returns false") {
        check(!uut.set_on_dispose(nullptr, make_single_shot_action([] {})));
      }
    }
    WHEN("the future is already disposed") {
      auto fut = uut.get_future();
      fut.dispose();
      THEN("set_on_dispose returns false") {
        check(!uut.set_on_dispose(nullptr, make_single_shot_action([] {})));
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
            promise_t copy{uut};
            check(fut.pending());
            // move ctor
            promise_t mv{std::move(copy)};
            check(fut.pending());
            {
              // copy assign
              promise_t copy2;
              copy2 = mv;
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
