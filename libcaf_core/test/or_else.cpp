/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/config.hpp"

#define CAF_SUITE or_else
#include "caf/test/unit_test.hpp"

#include "caf/all.hpp"

#define ERROR_HANDLER [&](error& err) { CAF_FAIL(system.render(err)); }

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
      .receive([](const std::string& str) { CAF_CHECK_EQUAL(str, "a"); },
               ERROR_HANDLER);
    self->request(testee, infinite, int16_t{1})
      .receive([](const std::string& str) { CAF_CHECK_EQUAL(str, "b"); },
               ERROR_HANDLER);
    self->request(testee, infinite, int32_t{1})
      .receive([](const std::string& str) { CAF_CHECK_EQUAL(str, "c"); },
               ERROR_HANDLER);
    self->send_exit(testee, exit_reason::user_shutdown);
  }
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(atom_tests, fixture)

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

CAF_TEST_FIXTURE_SCOPE_END()
