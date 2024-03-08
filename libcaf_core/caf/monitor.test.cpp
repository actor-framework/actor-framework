// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/event_based_actor.hpp"
#include "caf/exit_reason.hpp"

CAF_PUSH_DEPRECATED_WARNING

using namespace caf;

namespace {

WITH_FIXTURE(test::fixture::deterministic) {

TEST("monitoring another actor") {
  auto client_spawn = [](event_based_actor*) {
    return behavior{
      [](message) {},
    };
  };
  auto client1 = sys.spawn(client_spawn);
  auto client2 = sys.spawn(client_spawn);
  auto client3 = sys.spawn(client_spawn);
  SECTION("monitoring an actor invokes the down handler") {
    auto call_count = std::make_shared<int32_t>(0);
    auto observer = sys.spawn([=](event_based_actor* self) {
      self->monitor(client1);
      self->monitor(client2);
      self->monitor(client3);
      self->set_down_handler(
        [call_count](const down_msg&) { *call_count += 1; });
      return behavior{
        [](int32_t) {},
      };
    });
    inject_exit(client1);
    expect<down_msg>().with(std::ignore).from(client1).to(observer);
    check_eq(*call_count, 1);
    inject_exit(client2);
    expect<down_msg>().with(std::ignore).from(client2).to(observer);
    check_eq(*call_count, 2);
    inject_exit(client3);
    expect<down_msg>().with(std::ignore).from(client3).to(observer);
    check_eq(*call_count, 3);
  }
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
  SECTION("demonitoring an actor cancels the down handler") {
    auto call_count = std::make_shared<int32_t>(0);
    auto observer = sys.spawn([=](event_based_actor* self) {
      self->monitor(client1);
      self->monitor(client2);
      self->set_down_handler(
        [call_count](const down_msg&) { *call_count += 1; });
      self->demonitor(client1);
      return behavior{
        [](int32_t) {},
      };
    });
    inject_exit(client1);
    check_eq(mail_count(observer), 0u);
    check_eq(*call_count, 0);
    inject_exit(client2);
    expect<down_msg>().with(std::ignore).from(client2).to(observer);
    check_eq(*call_count, 1);
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

} // namespace

CAF_POP_WARNINGS
