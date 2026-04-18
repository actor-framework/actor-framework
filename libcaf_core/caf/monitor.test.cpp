// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/config.hpp"
#include "caf/event_based_actor.hpp"

using namespace caf;

WITH_FIXTURE(test::fixture::deterministic) {

TEST("monitoring another actor") {
  auto client_spawn = [](event_based_actor* self) {
    return behavior{
      [self](const exit_msg& msg) { self->quit(msg.reason); },
      [](message) {},
    };
  };
  auto client1 = sys.spawn(client_spawn);
  auto client2 = sys.spawn(client_spawn);
  auto client3 = sys.spawn(client_spawn);
  SECTION("monitoring with a callback") {
    auto call_count1 = std::make_shared<int32_t>(0);
    auto call_count2 = std::make_shared<int32_t>(0);
    auto call_count3 = std::make_shared<int32_t>(0);
    auto observer = sys.spawn([=](event_based_actor* self) {
      self->monitor(client1, [call_count1, client1](const error& reason) {
        *call_count1 += 1;
        test::runnable::current().check_eq(reason, exit_reason::user_shutdown);
      });
      self->monitor(client2, [call_count2, client2](const error& reason) {
        *call_count2 += 1;
        test::runnable::current().check_eq(reason, exit_reason::user_shutdown);
      });
      self->monitor(client3, [call_count3, client3](const error& reason) {
        *call_count3 += 1;
        test::runnable::current().check_eq(reason, exit_reason::user_shutdown);
      });
      return behavior{
        [](int32_t) {},
      };
    });
    inject_exit(client1);
    expect<action>().to(observer);
    check_eq(*call_count1, 1);
    check_eq(*call_count2, 0);
    check_eq(*call_count3, 0);
    inject_exit(client2);
    expect<action>().to(observer);
    check_eq(*call_count1, 1);
    check_eq(*call_count2, 1);
    check_eq(*call_count3, 0);
    inject_exit(client3);
    expect<action>().to(observer);
    check_eq(*call_count1, 1);
    check_eq(*call_count2, 1);
    check_eq(*call_count3, 1);
  }
  SECTION("duplicate monitors on one actor deliver two down messages") {
    auto call_count = std::make_shared<int32_t>(0);
    auto observer = sys.spawn([=](event_based_actor* self) {
      self->monitor(client1, [call_count](const error&) { *call_count += 1; });
      self->monitor(client1, [call_count](const error&) { *call_count += 1; });
      return behavior{
        [](int32_t) {},
      };
    });
    inject_exit(client1);
    expect<action>().to(observer);
    check_eq(*call_count, 1);
    expect<action>().to(observer);
    check_eq(*call_count, 2);
  }
  SECTION("disposing duplicate monitors clears them") {
    auto observer = sys.spawn([=](event_based_actor* self) {
      auto d1 = self->monitor(client1, [](const error&) {});
      auto d2 = self->monitor(client1, [](const error&) {});
      d1.dispose();
      d2.dispose();
      return behavior{
        [](int32_t) {},
      };
    });
    inject_exit(client1);
    check_eq(mail_count(observer), 0u);
  }
  SECTION("disposing a monitor cancels its callback") {
    auto call_count1 = std::make_shared<int32_t>(0);
    auto call_count2 = std::make_shared<int32_t>(0);
    auto observer = sys.spawn([=](event_based_actor* self) {
      auto d1 = self->monitor(client1, [call_count1](const error&) {
        *call_count1 += 1;
      });
      self->monitor(client2,
                    [call_count2](const error&) { *call_count2 += 1; });
      d1.dispose();
      return behavior{
        [](int32_t) {},
      };
    });
    inject_exit(client1);
    check_eq(mail_count(observer), 0u);
    check_eq(*call_count1, 0);
    inject_exit(client2);
    expect<action>().to(observer);
    check_eq(*call_count2, 1);
  }
  SECTION("canceling a monitor with a callback") {
    auto call_count1 = std::make_shared<int32_t>(0);
    auto call_count2 = std::make_shared<int32_t>(0);
    auto observer = sys.spawn([=](event_based_actor* self) {
      auto disp1 = self->monitor(client1, [call_count1](const error&) {
        *call_count1 += 1;
      });
      self->monitor(client2,
                    [call_count2](const error&) { *call_count2 += 1; });
      disp1.dispose();
      return behavior{
        [](int32_t) {},
      };
    });
    inject_exit(client1);
    check_eq(mail_count(observer), 0u);
    check_eq(*call_count1, 0);
    check_eq(*call_count2, 0);
    inject_exit(client2);
    expect<action>().to(observer);
    check_eq(*call_count1, 0);
    check_eq(*call_count2, 1);
  }
}

} // WITH_FIXTURE(test::fixture::deterministic)
