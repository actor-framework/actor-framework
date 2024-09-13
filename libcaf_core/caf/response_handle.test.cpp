// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/response_handle.hpp"

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/actor_from_state.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/result.hpp"
#include "caf/scheduled_actor/flow.hpp"
#include "caf/stateful_actor.hpp"
#include "caf/typed_actor.hpp"

using namespace caf;

namespace {

using i32_worker = typed_actor<result<int32_t>(int32_t)>;

struct dummy_state {
  static inline const char* name = "dummy";

  caf::behavior make_behavior() {
    return {
      [](int32_t x) -> result<int32_t> {
        if (x % 2 == 0)
          return x + x;
        else
          return make_error(sec::invalid_argument);
      },
    };
  }
};

struct fixture : test::fixture::deterministic {
  actor dummy;
  i32_worker typed_dummy;

  fixture() {
    auto dummy_fn = actor_from_state<dummy_state>;
    dummy = sys.spawn(dummy_fn);
    typed_dummy = actor_cast<i32_worker>(sys.spawn(dummy_fn));
    dispatch_messages();
  }
};

} // namespace

WITH_FIXTURE(fixture) {

SCENARIO("response handles are convertible to observables and singles") {
  GIVEN("a response handle with dynamic typing that produces a valid result") {
    WHEN("calling as_single") {
      THEN("observers see the result") {
        using result_t = std::variant<none_t, int32_t, caf::error>;
        result_t result;
        auto [self, launch] = sys.spawn_inactive();
        self->request(dummy, infinite, int32_t{42})
          .as_single<int32_t>()
          .subscribe([&result](int32_t val) { result = val; },
                     [&result](const error& what) { result = what; });
        auto aut = actor{self};
        launch();
        expect<int32_t>().with(42).from(aut).to(dummy);
        expect<int32_t>().with(84).from(dummy).to(aut);
        check(!mail_count());
        check_eq(result, result_t{84});
      }
    }
    WHEN("calling as_observable") {
      THEN("observers see the result") {
        using result_t = std::variant<none_t, int32_t, caf::error>;
        size_t on_next_calls = 0;
        bool completed = false;
        result_t result;
        auto [self, launch] = sys.spawn_inactive();
        self->request(dummy, infinite, int32_t{42})
          .as_observable<int32_t>()
          .do_on_error([&](const error& what) { result = what; })
          .do_on_complete([&] { completed = true; })
          .for_each([&](int32_t val) {
            result = val;
            ++on_next_calls;
          });
        auto aut = actor{self};
        launch();
        expect<int32_t>().with(42).from(aut).to(dummy);
        expect<int32_t>().with(84).from(dummy).to(aut);
        check(!mail_count());
        check_eq(result, result_t{84});
        check_eq(on_next_calls, 1u);
        check(completed);
      }
    }
  }
  GIVEN("a response handle with static typing that produces a valid result") {
    WHEN("calling as_single") {
      THEN("observers see the result") {
        using result_t = std::variant<none_t, int32_t, caf::error>;
        result_t result;
        auto [self, launch] = sys.spawn_inactive();
        self->request(typed_dummy, infinite, int32_t{42})
          .as_single<int32_t>()
          .subscribe([&result](int32_t val) { result = val; },
                     [&result](const error& what) { result = what; });
        auto aut = actor{self};
        launch();
        expect<int32_t>().with(42).from(aut).to(typed_dummy);
        expect<int32_t>().with(84).from(typed_dummy).to(aut);
        check(!mail_count());
        check_eq(result, result_t{84});
      }
    }
    WHEN("calling as_observable") {
      THEN("observers see the result") {
        using result_t = std::variant<none_t, int32_t, caf::error>;
        size_t on_next_calls = 0;
        bool completed = false;
        result_t result;
        auto [self, launch] = sys.spawn_inactive();
        self->request(typed_dummy, infinite, int32_t{42})
          .as_observable<int32_t>()
          .do_on_error([&](const error& what) { result = what; })
          .do_on_complete([&] { completed = true; })
          .for_each([&](int32_t val) {
            result = val;
            ++on_next_calls;
          });
        auto aut = actor{self};
        launch();
        expect<int32_t>().with(42).from(aut).to(typed_dummy);
        expect<int32_t>().with(84).from(typed_dummy).to(aut);
        check(!mail_count());
        check_eq(result, result_t{84});
        check_eq(on_next_calls, 1u);
        check(completed);
      }
    }
  }
  GIVEN("a response handle with dynamic typing that produces an error") {
    WHEN("calling as_single") {
      THEN("observers see an error") {
        using result_t = std::variant<none_t, int32_t, caf::error>;
        result_t result;
        auto [self, launch] = sys.spawn_inactive();
        self->request(dummy, infinite, int32_t{13})
          .as_single<int32_t>()
          .subscribe([&result](int32_t val) { result = val; },
                     [&result](const error& what) { result = what; });
        auto aut = actor{self};
        launch();
        expect<int32_t>().with(13).from(aut).to(dummy);
        expect<error>().from(dummy).to(aut);
        check(!mail_count());
        check_eq(result, result_t{make_error(sec::invalid_argument)});
      }
    }
    WHEN("calling as_observable") {
      THEN("observers see an error") {
        using result_t = std::variant<none_t, int32_t, caf::error>;
        size_t on_next_calls = 0;
        bool completed = false;
        result_t result;
        auto [self, launch] = sys.spawn_inactive();
        self->request(dummy, infinite, int32_t{13})
          .as_observable<int32_t>()
          .do_on_error([&](const error& what) { result = what; })
          .do_on_complete([&] { completed = true; })
          .for_each([&](int32_t val) {
            result = val;
            ++on_next_calls;
          });
        auto aut = actor{self};
        launch();
        expect<int32_t>().with(13).from(aut).to(dummy);
        expect<error>().from(dummy).to(aut);
        check(!mail_count());
        check_eq(result, result_t{make_error(sec::invalid_argument)});
        check_eq(on_next_calls, 0u);
        check(!completed);
      }
    }
  }
  GIVEN("a response handle with static typing that produces an error") {
    WHEN("calling as_single") {
      THEN("observers see an error") {
        using result_t = std::variant<none_t, int32_t, caf::error>;
        result_t result;
        auto [self, launch] = sys.spawn_inactive();
        self->request(typed_dummy, infinite, int32_t{13})
          .as_single<int32_t>()
          .subscribe([&result](int32_t val) { result = val; },
                     [&result](const error& what) { result = what; });
        auto aut = actor{self};
        launch();
        expect<int32_t>().with(13).from(aut).to(typed_dummy);
        expect<error>().from(typed_dummy).to(aut);
        check(!mail_count());
        check_eq(result, result_t{make_error(sec::invalid_argument)});
      }
    }
    WHEN("calling as_observable") {
      THEN("observers see an error") {
        using result_t = std::variant<none_t, int32_t, caf::error>;
        size_t on_next_calls = 0;
        bool completed = false;
        result_t result;
        auto [self, launch] = sys.spawn_inactive();
        self->request(typed_dummy, infinite, int32_t{13})
          .as_observable<int32_t>()
          .do_on_error([&](const error& what) { result = what; })
          .do_on_complete([&] { completed = true; })
          .for_each([&](int32_t val) {
            result = val;
            ++on_next_calls;
          });
        auto aut = actor{self};
        launch();
        expect<int32_t>().with(13).from(aut).to(typed_dummy);
        expect<error>().from(typed_dummy).to(aut);
        check(!mail_count());
        check_eq(result, result_t{make_error(sec::invalid_argument)});
        check_eq(on_next_calls, 0u);
        check(!completed);
      }
    }
  }
}

} // WITH_FIXTURE(fixture)
