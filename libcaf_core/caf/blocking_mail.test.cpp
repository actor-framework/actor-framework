// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/blocking_mail.hpp"

#include "caf/test/test.hpp"

#include "caf/actor_system_config.hpp"
#include "caf/blocking_actor.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/typed_actor.hpp"
#include "caf/typed_event_based_actor.hpp"

using namespace caf;
using namespace std::literals;

namespace {

using dummy_actor = typed_actor<result<int>(int)>;

using dummy_behavior = dummy_actor::behavior_type;

using i3_dummy_actor = typed_actor<result<int, int, int>(int)>;

using i3_dummy_behavior = i3_dummy_actor::behavior_type;

using void_dummy_actor = typed_actor<result<void>(int)>;

using void_dummy_behavior = void_dummy_actor::behavior_type;


struct config : actor_system_config {
  config() {
    set("caf.scheduler.max-threads", 1u);
  }
};

struct fixture {
  config cfg;
  actor_system sys{cfg};
};

WITH_FIXTURE(fixture) {

TEST("send request message") {
  scoped_actor self{sys};
  auto result = std::make_shared<int>(0);
  auto on_result = [result](int x) { *result = x; };
  auto err = std::make_shared<error>();
  auto on_error = [err](error x) { *err = x; };
  SECTION("valid response") {
    auto dummy = sys.spawn([]() -> dummy_behavior {
      return {
        [](int value) { return value * value; },
      };
    });
    SECTION("regular message") {
      self->mail(3).request(dummy, 1s).receive(on_result, on_error);
      check_eq(*result, 9);
    }
    SECTION("urgent message") {
      self->mail(3).urgent().request(dummy, 1s).receive(on_result, on_error);
      check_eq(*result, 9);
    }
  }
  SECTION("invalid response") {
    auto dummy = sys.spawn([]() -> behavior {
      return {
        [](int) { return "ok"s; },
      };
    });
    SECTION("regular message") {
      self->mail(3).request(dummy, 1s).receive(on_result, on_error);
      check_eq(*err, make_error(sec::unexpected_response));
    }
    SECTION("urgent message") {
      self->mail(3).urgent().request(dummy, 1s).receive(on_result, on_error);
      check_eq(*err, make_error(sec::unexpected_response));
    }
  }
  SECTION("no response") {
    auto dummy = sys.spawn([](event_based_actor* self) -> behavior {
      auto res = std::make_shared<response_promise>();
      return {
        [self, res](int) { *res = self->make_response_promise(); },
      };
    });
    self->mail(3).request(dummy, 10ms).receive(on_result, on_error);
    check_eq(*err, make_error(sec::request_timeout));
    self->mail(exit_msg{self->address(), exit_reason::user_shutdown})
      .send(dummy);
  }
}

TEST("send delayed request message") {
  scoped_actor self{sys};
  auto result = std::make_shared<int>(0);
  auto on_result = [result](int x) { *result = x; };
  auto err = std::make_shared<error>();
  auto on_error = [err](error x) { *err = x; };
  SECTION("valid response") {
    auto dummy = sys.spawn([]() -> dummy_behavior {
      return {
        [](int value) { return value * value; },
      };
    });
    SECTION("regular message") {
      self->mail(3)
        .delay(5ms)
        .request(dummy, 1s)
        .first //
        .receive(on_result, on_error);
      check_eq(*result, 9);
    }
    SECTION("urgent message") {
      self->mail(3)
        .urgent()
        .schedule(self->clock().now() + 5ms)
        .request(dummy, 1s)
        .first //
        .receive(on_result, on_error);
      check_eq(*result, 9);
    }
  }
  SECTION("invalid response") {
    auto dummy = sys.spawn([]() -> behavior {
      return {
        [](int) { return "ok"s; },
      };
    });
    SECTION("regular message") {
      self->mail(3)
        .delay(5ms)
        .request(dummy, 1s)
        .first //
        .receive(on_result, on_error);
      check_eq(*err, make_error(sec::unexpected_response));
    }
    SECTION("urgent message") {
      self->mail(3)
        .urgent()
        .schedule(self->clock().now() + 5ms)
        .request(dummy, 1s)
        .first //
        .receive(on_result, on_error);
      check_eq(*err, make_error(sec::unexpected_response));
    }
    SECTION("no response") {
      auto dummy = sys.spawn([](event_based_actor* self) -> behavior {
        auto res = std::make_shared<response_promise>();
        return {
          [self, res](int) { *res = self->make_response_promise(); },
        };
      });
      self->mail(3)
        .delay(5ms)
        .request(dummy, 10ms)
        .first //
        .receive(on_result, on_error);
      check_eq(*err, make_error(sec::request_timeout));
      self->mail(exit_msg{self->address(), exit_reason::user_shutdown})
        .send(dummy);
    }
  }
}

TEST("send request message to an invalid receiver") {
  scoped_actor self{sys};
  auto result = std::make_shared<int>(0);
  auto on_result = [result](int x) { *result = x; };
  auto err = std::make_shared<error>();
  auto on_error = [err](error x) { *err = x; };
  SECTION("regular message") {
    self->mail("hello world").request(actor{}, 1s).receive(on_result, on_error);
    check_eq(*result, 0);
    check_eq(*err, make_error(sec::invalid_request));
  }
  SECTION("delayed message") {
    self->mail("hello world")
      .delay(1s)
      .request(actor{}, 1s)
      .first.receive(on_result, on_error);
    check_eq(*result, 0);
    check_eq(*err, make_error(sec::invalid_request));
  }
}

TEST("using .receive on the response handle with a callback for expected") {
  scoped_actor self{sys};
  SECTION("valid response, statically typed") {
    auto result = std::make_shared<expected<int>>(0);
    auto dummy = sys.spawn([]() -> dummy_behavior {
      return {
        [](int value) { return value * value; },
      };
    });
    self->mail(3)
      .delay(5ms)
      .request(dummy, 1s)
      .first //
      .receive([result](auto res) {
        using res_t = decltype(res);
        static_assert(std::is_same_v<res_t, expected<int>>);
        *result = res;
      });
    if (check(result->has_value()))
      check_eq(*result, 9);
  }
  SECTION("invalid response, dynamically typed") {
    auto result = std::make_shared<expected<int>>(0);
    auto dummy = sys.spawn([]() -> behavior {
      return {
        [](int) -> caf::result<int> { return make_error(sec::runtime_error); },
      };
    });
    self->mail(3)
      .delay(5ms)
      .request(dummy, 1s)
      .first //
      .receive([result](expected<int> res) { *result = res; });
    if (check(!*result))
      check_eq(*result, make_error(sec::runtime_error));
  }
  SECTION("void return value, statically typed") {
    auto had_result = std::make_shared<bool>(false);
    auto dummy = sys.spawn([]() -> void_dummy_behavior {
      return {
        [](int) {},
      };
    });
    self->mail(3)
      .delay(5ms)
      .request(dummy, 1s)
      .first //
      .receive([had_result](auto res) {
        using res_t = decltype(res);
        static_assert(std::is_same_v<res_t, expected<void>>);
        *had_result = res.has_value();
      });
    check(*had_result);
  }
  SECTION("void return value, dynamically typed") {
    auto had_result = std::make_shared<bool>(false);
    auto dummy = sys.spawn([]() -> behavior {
      return {
        [](int) {},
      };
    });
    self->mail(3)
      .delay(5ms)
      .request(dummy, 1s)
      .first //
      .receive([had_result](expected<void> res) {
        using res_t = decltype(res);
        static_assert(std::is_same_v<res_t, expected<void>>);
        *had_result = res.has_value();
      });
    check(*had_result);
  }
  SECTION("multiple return values, statically typed") {
    using tuple_t = std::tuple<int, int, int>;
    auto result = std::make_shared<expected<tuple_t>>(tuple_t{0, 0, 0});
    auto dummy = sys.spawn([]() -> i3_dummy_behavior {
      return {
        [](int) -> caf::result<int, int, int> {
          return {1, 2, 3};
        },
      };
    });
    self->mail(3)
      .delay(5ms)
      .request(dummy, 1s)
      .first //
      .receive([result](auto res) {
        using res_t = decltype(res);
        static_assert(std::is_same_v<res_t, expected<tuple_t>>);
        *result = res;
      });
    if (check(result->has_value()))
      check_eq(*result, std::tuple{1, 2, 3});
  }
  SECTION("multiple return values, dynamically typed") {
    using tuple_t = std::tuple<int, int, int>;
    auto result = std::make_shared<expected<tuple_t>>(tuple_t{0, 0, 0});
    auto dummy = sys.spawn([]() -> behavior {
      return {
        [](int) -> caf::result<int, int, int> {
          return {1, 2, 3};
        },
      };
    });
    self->mail(3)
      .delay(5ms)
      .request(dummy, 1s)
      .first //
      .receive([result](expected<tuple_t> res) {
        using res_t = decltype(res);
        static_assert(std::is_same_v<res_t, expected<tuple_t>>);
        *result = res;
      });
    if (check(result->has_value()))
      check_eq(*result, std::tuple{1, 2, 3});
  }
}

} // WITH_FIXTURE(fixture)

} // namespace
