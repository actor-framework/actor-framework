// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE behavior

#include "caf/behavior.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/config.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/message_handler.hpp"
#include "caf/send.hpp"

#include "core-test.hpp"

#include <functional>
#include <string_view>

using namespace caf;

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
  std::optional<int32_t> res_of(behavior& bhvr, message& msg) {
    auto res = bhvr(msg);
    if (!res)
      return std::nullopt;
    if (auto view = make_const_typed_message_view<int32_t>(*res))
      return get<0>(view);
    return std::nullopt;
  }

  message m1 = make_message(int32_t{1});

  message m2 = make_message(int32_t{1}, int32_t{2});

  message m3 = make_message(int32_t{1}, int32_t{2}, int32_t{3});
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

CAF_TEST(default_construct) {
  behavior f;
  CHECK_EQ(f(m1), std::nullopt);
  CHECK_EQ(f(m2), std::nullopt);
  CHECK_EQ(f(m3), std::nullopt);
}

CAF_TEST(nocopy_function_object) {
  behavior f{nocopy_fun{}};
  CHECK_EQ(f(m1), std::nullopt);
  CHECK_EQ(res_of(f, m2), 3);
  CHECK_EQ(f(m3), std::nullopt);
}

CAF_TEST(single_lambda_construct) {
  behavior f{[](int x) { return x + 1; }};
  CHECK_EQ(res_of(f, m1), 2);
  CHECK_EQ(res_of(f, m2), std::nullopt);
  CHECK_EQ(res_of(f, m3), std::nullopt);
}

CAF_TEST(multiple_lambda_construct) {
  behavior f{[](int x) { return x + 1; }, [](int x, int y) { return x * y; }};
  CHECK_EQ(res_of(f, m1), 2);
  CHECK_EQ(res_of(f, m2), 2);
  CHECK_EQ(res_of(f, m3), std::nullopt);
}

CAF_TEST(become_empty_behavior) {
  actor_system_config cfg{};
  actor_system sys{cfg};
  auto make_bhvr = [](event_based_actor* self) -> behavior {
    return {
      [=](int) { self->become(behavior{}); },
    };
  };
  anon_send(sys.spawn(make_bhvr), int{5});
}

END_FIXTURE_SCOPE()
