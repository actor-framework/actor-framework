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

#define ERROR_HANDLER                                                          \
  [&](error& err) { CAF_FAIL(system.render(err)); }

using namespace caf;

namespace {

using a_atom = atom_constant<atom("a")>;
using b_atom = atom_constant<atom("b")>;
using c_atom = atom_constant<atom("c")>;

message_handler handle_a() {
  return [](a_atom) { return 1; };
}

message_handler handle_b() {
  return [](b_atom) { return 2; };
}

message_handler handle_c() {
  return [](c_atom) { return 3; };
}

struct fixture {
  fixture() : system(cfg) {
    // nop
  }

  actor_system_config cfg;
  actor_system system;

  void run_testee(const actor& testee) {
    scoped_actor self{system};
    self->request(testee, infinite, a_atom::value).receive(
      [](int i) {
        CAF_CHECK_EQUAL(i, 1);
      },
      ERROR_HANDLER
    );
    self->request(testee, infinite, b_atom::value).receive(
      [](int i) {
        CAF_CHECK_EQUAL(i, 2);
      },
      ERROR_HANDLER
    );
    self->request(testee, infinite, c_atom::value).receive(
      [](int i) {
        CAF_CHECK_EQUAL(i, 3);
      },
      ERROR_HANDLER
    );
    self->send_exit(testee, exit_reason::user_shutdown);
    self->await_all_other_actors_done();
  }
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(atom_tests, fixture)

CAF_TEST(composition1) {
  run_testee(
    system.spawn([=] {
      return handle_a().or_else(handle_b()).or_else(handle_c());
    })
  );
}

CAF_TEST(composition2) {
  run_testee(
    system.spawn([=] {
      return handle_a().or_else(handle_b()).or_else([](c_atom) { return 3; });
    })
  );
}

CAF_TEST(composition3) {
  run_testee(
    system.spawn([=] {
      return message_handler{[](a_atom) { return 1; }}.
             or_else(handle_b()).or_else(handle_c());
    })
  );
}

CAF_TEST_FIXTURE_SCOPE_END()
