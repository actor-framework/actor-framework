// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE or_else

#include "core-test.hpp"

#include "caf/all.hpp"

#define ERROR_HANDLER [&](error& err) { CAF_FAIL(err); }

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
    self->request(testee, infinite, int8_t{1})
      .receive([](const std::string& str) { CHECK_EQ(str, "a"); },
               ERROR_HANDLER);
    self->request(testee, infinite, int16_t{1})
      .receive([](const std::string& str) { CHECK_EQ(str, "b"); },
               ERROR_HANDLER);
    self->request(testee, infinite, int32_t{1})
      .receive([](const std::string& str) { CHECK_EQ(str, "c"); },
               ERROR_HANDLER);
    self->send_exit(testee, exit_reason::user_shutdown);
  }
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

CAF_TEST(composition1) {
  run_testee(system.spawn(
    [=] { return handle_a().or_else(handle_b()).or_else(handle_c()); }));
}

CAF_TEST(composition2) {
  run_testee(system.spawn([=] {
    return handle_a().or_else(handle_b()).or_else([](int32_t) { return "c"; });
  }));
}

CAF_TEST(composition3) {
  run_testee(system.spawn([=] {
    return message_handler{[](int8_t) { return "a"; }}
      .or_else(handle_b())
      .or_else(handle_c());
  }));
}

END_FIXTURE_SCOPE()
