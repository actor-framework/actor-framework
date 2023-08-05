// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE response_handle

#include "caf/response_handle.hpp"

#include "caf/scheduled_actor/flow.hpp"

#include "core-test.hpp"

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

using dummy_actor = stateful_actor<dummy_state>;

struct fixture : test_coordinator_fixture<> {
  actor dummy;
  i32_worker typed_dummy;

  fixture() {
    dummy = sys.spawn<dummy_actor>();
    typed_dummy = actor_cast<i32_worker>(sys.spawn<dummy_actor>());
    sched.run();
  }
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

SCENARIO("response handles are convertible to observables and singles") {
  GIVEN("a response handle with dynamic typing that produces a valid result") {
    WHEN("calling as_single") {
      THEN("observers see the result") {
        using result_t = std::variant<none_t, int32_t, caf::error>;
        result_t result;
        auto [self, launch] = sys.spawn_inactive<event_based_actor>();
        self->request(dummy, infinite, int32_t{42})
          .as_single<int32_t>()
          .subscribe([&result](int32_t val) { result = val; },
                     [&result](const error& what) { result = what; });
        auto aut = actor{self};
        launch();
        expect((int32_t), from(aut).to(dummy).with(42));
        expect((int32_t), from(dummy).to(aut).with(84));
        CHECK(!sched.has_job());
        CHECK_EQ(result, result_t{84});
      }
    }
    WHEN("calling as_observable") {
      THEN("observers see the result") {
        using result_t = std::variant<none_t, int32_t, caf::error>;
        size_t on_next_calls = 0;
        bool completed = false;
        result_t result;
        auto [self, launch] = sys.spawn_inactive<event_based_actor>();
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
        expect((int32_t), from(aut).to(dummy).with(42));
        expect((int32_t), from(dummy).to(aut).with(84));
        CHECK(!sched.has_job());
        CHECK_EQ(result, result_t{84});
        CHECK_EQ(on_next_calls, 1u);
        CHECK(completed);
      }
    }
  }
  GIVEN("a response handle with static typing that produces a valid result") {
    WHEN("calling as_single") {
      THEN("observers see the result") {
        using result_t = std::variant<none_t, int32_t, caf::error>;
        result_t result;
        auto [self, launch] = sys.spawn_inactive<event_based_actor>();
        self->request(typed_dummy, infinite, int32_t{42})
          .as_single<int32_t>()
          .subscribe([&result](int32_t val) { result = val; },
                     [&result](const error& what) { result = what; });
        auto aut = actor{self};
        launch();
        expect((int32_t), from(aut).to(typed_dummy).with(42));
        expect((int32_t), from(typed_dummy).to(aut).with(84));
        CHECK(!sched.has_job());
        CHECK_EQ(result, result_t{84});
      }
    }
    WHEN("calling as_observable") {
      THEN("observers see the result") {
        using result_t = std::variant<none_t, int32_t, caf::error>;
        size_t on_next_calls = 0;
        bool completed = false;
        result_t result;
        auto [self, launch] = sys.spawn_inactive<event_based_actor>();
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
        expect((int32_t), from(aut).to(typed_dummy).with(42));
        expect((int32_t), from(typed_dummy).to(aut).with(84));
        CHECK(!sched.has_job());
        CHECK_EQ(result, result_t{84});
        CHECK_EQ(on_next_calls, 1u);
        CHECK(completed);
      }
    }
  }
  GIVEN("a response handle with dynamic typing that produces an error") {
    WHEN("calling as_single") {
      THEN("observers see an error") {
        using result_t = std::variant<none_t, int32_t, caf::error>;
        result_t result;
        auto [self, launch] = sys.spawn_inactive<event_based_actor>();
        self->request(dummy, infinite, int32_t{13})
          .as_single<int32_t>()
          .subscribe([&result](int32_t val) { result = val; },
                     [&result](const error& what) { result = what; });
        auto aut = actor{self};
        launch();
        expect((int32_t), from(aut).to(dummy).with(13));
        expect((error), from(dummy).to(aut));
        CHECK(!sched.has_job());
        CHECK_EQ(result, result_t{make_error(sec::invalid_argument)});
      }
    }
    WHEN("calling as_observable") {
      THEN("observers see an error") {
        using result_t = std::variant<none_t, int32_t, caf::error>;
        size_t on_next_calls = 0;
        bool completed = false;
        result_t result;
        auto [self, launch] = sys.spawn_inactive<event_based_actor>();
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
        expect((int32_t), from(aut).to(dummy).with(13));
        expect((error), from(dummy).to(aut));
        CHECK(!sched.has_job());
        CHECK_EQ(result, result_t{make_error(sec::invalid_argument)});
        CHECK_EQ(on_next_calls, 0u);
        CHECK(!completed);
      }
    }
  }
  GIVEN("a response handle with static typing that produces an error") {
    WHEN("calling as_single") {
      THEN("observers see an error") {
        using result_t = std::variant<none_t, int32_t, caf::error>;
        result_t result;
        auto [self, launch] = sys.spawn_inactive<event_based_actor>();
        self->request(typed_dummy, infinite, int32_t{13})
          .as_single<int32_t>()
          .subscribe([&result](int32_t val) { result = val; },
                     [&result](const error& what) { result = what; });
        auto aut = actor{self};
        launch();
        expect((int32_t), from(aut).to(typed_dummy).with(13));
        expect((error), from(typed_dummy).to(aut));
        CHECK(!sched.has_job());
        CHECK_EQ(result, result_t{make_error(sec::invalid_argument)});
      }
    }
    WHEN("calling as_observable") {
      THEN("observers see an error") {
        using result_t = std::variant<none_t, int32_t, caf::error>;
        size_t on_next_calls = 0;
        bool completed = false;
        result_t result;
        auto [self, launch] = sys.spawn_inactive<event_based_actor>();
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
        expect((int32_t), from(aut).to(typed_dummy).with(13));
        expect((error), from(typed_dummy).to(aut));
        CHECK(!sched.has_job());
        CHECK_EQ(result, result_t{make_error(sec::invalid_argument)});
        CHECK_EQ(on_next_calls, 0u);
        CHECK(!completed);
      }
    }
  }
}

END_FIXTURE_SCOPE()
