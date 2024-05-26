// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/test/test.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/response_handle.hpp"
#include "caf/scoped_actor.hpp"

using namespace caf;

namespace {

message_handler handle_a() {
  return {
    [](int8_t) { return "a"; },
  };
}

message_handler handle_b() {
  return {
    [](int16_t) { return "b"; },
  };
}

message_handler handle_c() {
  return {
    [](int32_t) { return "c"; },
  };
}

struct fixture {
  fixture() : system(cfg) {
    // nop
  }

  actor_system_config cfg;
  actor_system system;

  void run_testee(const actor& testee) {
    scoped_actor self{system};
    const auto& error_handler = [&](error& err) {
      test::runnable::current().fail("{}", err);
    };
    auto& this_test = test::runnable::current();
    self->mail(int8_t{1})
      .request(testee, infinite)
      .receive(
        [&this_test](const std::string& str) { //
          this_test.check_eq(str, "a");
        },
        error_handler);
    self->mail(int16_t{1})
      .request(testee, infinite)
      .receive(
        [&this_test](const std::string& str) { //
          this_test.check_eq(str, "b");
        },
        error_handler);
    self->mail(int32_t{1})
      .request(testee, infinite)
      .receive(
        [&this_test](const std::string& str) { //
          this_test.check_eq(str, "c");
        },
        error_handler);
    self->send_exit(testee, exit_reason::user_shutdown);
  }
};

} // namespace

WITH_FIXTURE(fixture) {

TEST("composition1") {
  run_testee(system.spawn(
    [=] { return handle_a().or_else(handle_b()).or_else(handle_c()); }));
}

TEST("composition2") {
  run_testee(system.spawn([=] {
    return handle_a().or_else(handle_b()).or_else([](int32_t) { return "c"; });
  }));
}

TEST("composition3") {
  run_testee(system.spawn([=] {
    return message_handler{[](int8_t) { return "a"; }}
      .or_else(handle_b())
      .or_else(handle_c());
  }));
}

} // WITH_FIXTURE(fixture)
