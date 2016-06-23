/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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

using msg_atom = atom_constant<atom("msg")>;
using timeout_atom = atom_constant<atom("timeout")>;

behavior testee(event_based_actor* self) {
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

CAF_TEST(test_local_group) {
  actor_system system;
  system.spawn(testee);
}
