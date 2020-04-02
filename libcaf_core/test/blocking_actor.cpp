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

#define CAF_SUITE blocking_actor

#include "caf/blocking_actor.hpp"

#include "core-test.hpp"

using namespace caf;

namespace {

struct fixture {
  actor_system_config cfg;
  actor_system system;
  scoped_actor self;

  fixture() : system(cfg), self(system) {
    // nop
  }
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(blocking_actor_tests, fixture)

CAF_TEST(catch_all) {
  self->send(self, 42);
  self->receive([](float) { CAF_FAIL("received unexpected float"); },
                others >> [](message& msg) -> skippable_result {
                  CAF_CHECK_EQUAL(to_string(msg), "(42)");
                  return make_error(sec::unexpected_message);
                });
  self->receive(
    [](const error& err) {
      CAF_CHECK_EQUAL(err, sec::unexpected_message);
    }
  );
}

CAF_TEST(behavior_ref) {
  behavior bhvr{
    [](int i) {
      CAF_CHECK_EQUAL(i, 42);
    }
  };
  self->send(self, 42);
  self->receive(bhvr);
}

CAF_TEST(timeout_in_scoped_actor) {
  bool timeout_called = false;
  scoped_actor self{system};
  self->receive(
    after(std::chrono::milliseconds(20)) >> [&] {
      timeout_called = true;
    }
  );
  CAF_CHECK(timeout_called);
}

CAF_TEST_FIXTURE_SCOPE_END()
