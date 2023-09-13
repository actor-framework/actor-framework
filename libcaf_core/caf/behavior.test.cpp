// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/behavior.hpp"

#include "caf/test/caf_test_main.hpp"
#include "caf/test/test.hpp"

using namespace caf;

class nocopy_fun {
public:
  nocopy_fun() = default;

  nocopy_fun(nocopy_fun&&) = default;

  nocopy_fun& operator=(nocopy_fun&&) = default;

  nocopy_fun(const nocopy_fun&) = delete;

  nocopy_fun& operator=(const nocopy_fun&) = delete;

  int32_t operator()(int32_t x, int32_t y) {
    return x + y;
  }
};

struct fixture {
  std::optional<int> res_of(behavior& bhvr, message& msg) {
    auto res = bhvr(msg);
    if (!res)
      return std::nullopt;
    if (auto view = make_const_typed_message_view<int>(*res))
      return get<0>(view);
    return std::nullopt;
  }

  message m1 = make_message(1);

  message m2 = make_message(1, 2);

  message m3 = make_message(1, 2, 3);
};

WITH_FIXTURE(fixture) {

TEST("a default-constructed behavior accepts no messages") {
  auto f = behavior{};
  check_eq(res_of(f, m1), std::nullopt);
  check_eq(res_of(f, m2), std::nullopt);
  check_eq(res_of(f, m3), std::nullopt);
}

TEST("behaviors may be constructed from lambda expressions") {
  SECTION("single argument") {
    auto f = behavior{
      [](int x) { return x + 1; },
    };
    check_eq(res_of(f, m1), 2);
    check_eq(res_of(f, m2), std::nullopt);
    check_eq(res_of(f, m3), std::nullopt);
  }
  SECTION("multiple arguments") {
    auto f = behavior{
      [](int x) { return x + 1; },
      [](int x, int y) { return x + y; },
    };
    check_eq(res_of(f, m1), 2);
    check_eq(res_of(f, m2), 3);
    check_eq(res_of(f, m3), std::nullopt);
  }
}

TEST("behaviors may be constructed from move-only types") {
  auto f = behavior{nocopy_fun{}};
  check_eq(res_of(f, m1), std::nullopt);
  check_eq(res_of(f, m2), 3);
  check_eq(res_of(f, m3), std::nullopt);
}

TEST("the first matching handler wins") {
  auto f1_called = std::make_shared<bool>(false);
  auto f2_called = std::make_shared<bool>(false);
  auto f = behavior{
    [f1_called](int x) {
      *f1_called = true;
      return x + 1;
    },
    [f2_called](int x) {
      *f2_called = true;
      return x + 2;
    },
  };
  check_eq(res_of(f, m1), 2);
  check(*f1_called);
  check(!*f2_called);
}

TEST("a handler that takes a message argument is a catch-all handler") {
  SECTION("using signature (message)") {
    auto f = behavior{
      [](message) { return 1; },
      [](int) { return 2; },
      [](int, int) { return 3; },
      [](int, int, int) { return 4; },
    };
    check_eq(res_of(f, m1), 1);
    check_eq(res_of(f, m2), 1);
    check_eq(res_of(f, m3), 1);
  }
  SECTION("using signature (message&)") {
    auto f = behavior{
      [](message&) { return 1; },
      [](int) { return 2; },
      [](int, int) { return 3; },
      [](int, int, int) { return 4; },
    };
    check_eq(res_of(f, m1), 1);
    check_eq(res_of(f, m2), 1);
    check_eq(res_of(f, m3), 1);
  }
  SECTION("using signature (const message&)") {
    auto f = behavior{
      [](const message&) { return 1; },
      [](int) { return 2; },
      [](int, int) { return 3; },
      [](int, int, int) { return 4; },
    };
    check_eq(res_of(f, m1), 1);
    check_eq(res_of(f, m2), 1);
    check_eq(res_of(f, m3), 1);
  }
  SECTION("returning void from a catch-all handler") {
    auto cb_id = std::make_shared<int>(0);
    auto f = behavior{
      [cb_id](message&) { *cb_id = 1; },
      [cb_id](int) { *cb_id = 2; },
      [cb_id](int, int) { *cb_id = 3; },
      [cb_id](int, int, int) { *cb_id = 4; },
    };
    auto call_with = [cb_id, &f](message& msg) {
      *cb_id = 0;
      f(msg);
      return *cb_id;
    };
    check_eq(call_with(m1), 1);
    check_eq(call_with(m2), 1);
    check_eq(call_with(m3), 1);
  }
}

} // WITH_FIXTURE(fixture)

CAF_TEST_MAIN()
