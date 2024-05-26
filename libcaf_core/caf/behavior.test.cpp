// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/behavior.hpp"

#include "caf/test/test.hpp"

#include "caf/cow_string.hpp"
#include "caf/message_handler.hpp"

using namespace caf;
using namespace std::literals;

namespace {

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

TEST("mutable references in a message handler forces a message to detach") {
  auto str = cow_string{"hello"s};
  auto msg = make_message(str);
  require_eq(str.get_reference_count(), 2u);
  SECTION("a unique messages simply passes its argument by mutable reference") {
    auto called = false;
    auto f = behavior{
      [this, &str, &called](cow_string& val) {
        called = true;
        check_eq(str.c_str(), val.c_str());
        require_eq(str.get_reference_count(), 2u);
      },
    };
    f(msg);
    check(called);
  }
  SECTION("a shared messages detaches its content") {
    auto msg_copy = msg;
    auto called = false;
    auto f = behavior{
      [this, &str, &called](cow_string& val) {
        called = true;
        check_eq(str.c_str(), val.c_str());
        require_eq(str.get_reference_count(), 3u);
      },
    };
    f(msg_copy);
    check(msg_copy.unique());
    check(called);
  }
}

TEST("unique message auto-move their arguments") {
  auto str = cow_string{"hello"s};
  auto msg = make_message(str);
  require_eq(str.get_reference_count(), 2u);
  SECTION("values are moved if a message is unique") {
    auto called = false;
    auto f = behavior{
      [this, &str, &called](cow_string val) {
        called = true;
        check_eq(str.c_str(), val.c_str());
        require_eq(str.get_reference_count(), 2u);
      },
    };
    f(msg);
    check(called);
  }
  SECTION("values are copied if a message is shared") {
    auto msg_copy = msg;
    auto called = false;
    auto f = behavior{
      [this, &str, &called](cow_string val) {
        called = true;
        check_eq(str.c_str(), val.c_str());
        // One reference from `str`, one from the shared message, and one from
        // `val`.
        require_eq(str.get_reference_count(), 3u);
      },
    };
    f(msg_copy);
    check_eq(msg_copy.cptr(), msg.cptr());
    check(called);
  }
}

TEST("message handlers with const arguments never force a detach or copy") {
  auto str = cow_string{"hello"s};
  auto msg = make_message(str);
  require_eq(str.get_reference_count(), 2u);
  SECTION("values are passed along if a message is unique") {
    auto called = false;
    auto f = behavior{
      [this, &str, &called](const cow_string& val) {
        called = true;
        check_eq(str.c_str(), val.c_str());
        require_eq(str.get_reference_count(), 2u);
      },
    };
    f(msg);
    check(called);
  }
  SECTION("values are passed along if a message is shared") {
    auto msg_copy = msg;
    auto called = false;
    auto f = behavior{
      [this, &str, &called](const cow_string& val) {
        called = true;
        check_eq(str.c_str(), val.c_str());
        require_eq(str.get_reference_count(), 2u);
      },
    };
    f(msg_copy);
    check_eq(msg_copy.cptr(), msg.cptr());
    check(called);
  }
}

TEST("behaviors may be assigned after construction") {
  auto f = behavior{
    [](int x) { return x + 1; },
  };
  SECTION("assigned with another behavior") {
    auto g = behavior{
      [](int x) { return x + 2; },
    };
    f.assign(g);
    check_eq(res_of(f, m1), 3);
    check_eq(res_of(f, m2), std::nullopt);
    check_eq(res_of(f, m3), std::nullopt);
    check_eq(res_of(g, m1), 3);
    check_eq(res_of(g, m2), std::nullopt);
    check_eq(res_of(g, m3), std::nullopt);
  }
  SECTION("assigned with another message_handler") {
    auto g = [] {
      return message_handler{
        [](int x) { return x + 2; },
      };
    };
    f.assign(g());
    check_eq(res_of(f, m1), 3);
    check_eq(res_of(f, m2), std::nullopt);
    check_eq(res_of(f, m3), std::nullopt);
  }
}

} // WITH_FIXTURE(fixture)

} // namespace
