/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
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

#define CAF_SUITE local_group
#include "caf/test/unit_test.hpp"

#include <chrono>

#include "caf/all.hpp"

using namespace caf;

namespace {

using msg_atom = atom_constant<atom("msg")>;
using timeout_atom = atom_constant<atom("timeout")>;

class testee1 : public event_based_actor {
public:
  testee1(actor_config& cfg) : event_based_actor(cfg), x_(0) {
    // nop
  }

  behavior make_behavior() override {
    return {
      [=](put_atom, int x) { x_ = x; },
      [=](get_atom) { return x_; }
    };
  }

private:
  int x_;
};


behavior testee2(event_based_actor* self) {
  auto counter = std::make_shared<int>(0);
  auto grp = self->system().groups().get_local("test");
  self->join(grp);
  CAF_MESSAGE("self joined group");
  self->send(grp, msg_atom::value);
  return {
    [=](msg_atom) {
      CAF_MESSAGE("received `msg_atom`");
      ++*counter;
      self->leave(grp);
      self->send(grp, msg_atom::value);
      self->send(self, timeout_atom::value);
    },
    [=](timeout_atom) {
      // this actor should receive only 1 message
      CAF_CHECK_EQUAL(*counter, 1);
      self->quit();
    }
  };
}

struct fixture {
  actor_system_config config;
  actor_system system{config};
  scoped_actor self{system};
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(group_tests, fixture)

CAF_TEST(function_based_self_joining) {
  system.spawn(testee2);
}

CAF_TEST(class_based_joined_at_spawn) {
  auto grp = system.groups().get_local("test");
  auto tst = system.spawn_in_group<testee1>(grp);
  self->send(grp, put_atom::value, 42);
  self->request(tst, infinite, get_atom::value).receive(
    [&](int x) {
      CAF_REQUIRE_EQUAL(x, 42);
    },
    [&](const error& e) {
      CAF_FAIL("error: " << system.render(e));
    }
  );
  // groups holds a reference to testee, stop manually
  self->send_exit(tst, exit_reason::user_shutdown);
}

CAF_TEST_FIXTURE_SCOPE_END()

