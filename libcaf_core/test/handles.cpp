/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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

// This unit test checks guarantees regarding ordering and equality for actor
// handles, i.e., actor_addr, actor, and typed_actor<...>.

#include "caf/config.hpp"

#define CAF_SUITE handles
#include "caf/test/unit_test.hpp"

#include "caf/all.hpp"

using namespace caf;

namespace {

// Simple int32_terface for testee actors.
using testee_actor = typed_actor<replies_to<int32_t>::with<int32_t>>;

// Dynamically typed testee.
behavior dt_testee() {
  return {
    [](int32_t x) {
      return x * x;
    }
  };
}

// Statically typed testee.
testee_actor::behavior_type st_testee() {
  return {
    [](int32_t x) {
      return x * x;
    }
  };
}

// A simple wrapper for storing a handle in all representations.
struct handle_set {
  // Weak handle to the actor.
  actor_addr wh;
  // Dynamically typed handle to the actor.
  actor dt;
  // Staically typed handle to the actor.
  testee_actor st;

  handle_set() = default;

  template <class T>
  handle_set(const T& hdl)
      : wh(hdl.address()),
        dt(actor_cast<actor>(hdl)),
        st(actor_cast<testee_actor>(hdl)) {
    // nop
  }
};

struct fixture {
  fixture()
      : sys(cfg),
        self(sys, true),
        a1{sys.spawn(dt_testee)},
        a2{sys.spawn(st_testee)} {
    // nop
  }

  actor_system_config cfg;
  actor_system sys;
  scoped_actor self;
  handle_set a0;
  handle_set a1{sys.spawn(dt_testee)};
  handle_set a2{sys.spawn(st_testee)};
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(handle_tests, fixture)

CAF_TEST(identity) {
  // all handles in a0 are equal
  CAF_CHECK_EQUAL(a0.wh, a0.wh);
  CAF_CHECK_EQUAL(a0.wh, a0.dt);
  CAF_CHECK_EQUAL(a0.wh, a0.st);
  CAF_CHECK_EQUAL(a0.dt, a0.wh);
  CAF_CHECK_EQUAL(a0.dt, a0.dt);
  CAF_CHECK_EQUAL(a0.dt, a0.st);
  CAF_CHECK_EQUAL(a0.st, a0.wh);
  CAF_CHECK_EQUAL(a0.st, a0.dt);
  CAF_CHECK_EQUAL(a0.st, a0.st);
  // all handles in a1 are equal
  CAF_CHECK_EQUAL(a1.wh, a1.wh);
  CAF_CHECK_EQUAL(a1.wh, a1.dt);
  CAF_CHECK_EQUAL(a1.wh, a1.st);
  CAF_CHECK_EQUAL(a1.dt, a1.wh);
  CAF_CHECK_EQUAL(a1.dt, a1.dt);
  CAF_CHECK_EQUAL(a1.dt, a1.st);
  CAF_CHECK_EQUAL(a1.st, a1.wh);
  CAF_CHECK_EQUAL(a1.st, a1.dt);
  CAF_CHECK_EQUAL(a1.st, a1.st);
  // all handles in a2 are equal
  CAF_CHECK_EQUAL(a2.wh, a2.wh);
  CAF_CHECK_EQUAL(a2.wh, a2.dt);
  CAF_CHECK_EQUAL(a2.wh, a2.st);
  CAF_CHECK_EQUAL(a2.dt, a2.wh);
  CAF_CHECK_EQUAL(a2.dt, a2.dt);
  CAF_CHECK_EQUAL(a2.dt, a2.st);
  CAF_CHECK_EQUAL(a2.st, a2.wh);
  CAF_CHECK_EQUAL(a2.st, a2.dt);
  CAF_CHECK_EQUAL(a2.st, a2.st);
  // all handles in a0 are *not* equal to any handle in a1 or a2
  CAF_CHECK_NOT_EQUAL(a0.wh, a1.wh);
  CAF_CHECK_NOT_EQUAL(a0.wh, a1.dt);
  CAF_CHECK_NOT_EQUAL(a0.wh, a1.st);
  CAF_CHECK_NOT_EQUAL(a0.dt, a1.wh);
  CAF_CHECK_NOT_EQUAL(a0.dt, a1.dt);
  CAF_CHECK_NOT_EQUAL(a0.dt, a1.st);
  CAF_CHECK_NOT_EQUAL(a0.st, a1.wh);
  CAF_CHECK_NOT_EQUAL(a0.st, a1.dt);
  CAF_CHECK_NOT_EQUAL(a0.st, a1.st);
  CAF_CHECK_NOT_EQUAL(a0.wh, a2.wh);
  CAF_CHECK_NOT_EQUAL(a0.wh, a2.dt);
  CAF_CHECK_NOT_EQUAL(a0.wh, a2.st);
  CAF_CHECK_NOT_EQUAL(a0.dt, a2.wh);
  CAF_CHECK_NOT_EQUAL(a0.dt, a2.dt);
  CAF_CHECK_NOT_EQUAL(a0.dt, a2.st);
  CAF_CHECK_NOT_EQUAL(a0.st, a2.wh);
  CAF_CHECK_NOT_EQUAL(a0.st, a2.dt);
  CAF_CHECK_NOT_EQUAL(a0.st, a2.st);
  // all handles in a1 are *not* equal to any handle in a0 or a2
  CAF_CHECK_NOT_EQUAL(a1.wh, a0.wh);
  CAF_CHECK_NOT_EQUAL(a1.wh, a0.dt);
  CAF_CHECK_NOT_EQUAL(a1.wh, a0.st);
  CAF_CHECK_NOT_EQUAL(a1.dt, a0.wh);
  CAF_CHECK_NOT_EQUAL(a1.dt, a0.dt);
  CAF_CHECK_NOT_EQUAL(a1.dt, a0.st);
  CAF_CHECK_NOT_EQUAL(a1.st, a0.wh);
  CAF_CHECK_NOT_EQUAL(a1.st, a0.dt);
  CAF_CHECK_NOT_EQUAL(a1.st, a0.st);
  CAF_CHECK_NOT_EQUAL(a1.wh, a2.wh);
  CAF_CHECK_NOT_EQUAL(a1.wh, a2.dt);
  CAF_CHECK_NOT_EQUAL(a1.wh, a2.st);
  CAF_CHECK_NOT_EQUAL(a1.dt, a2.wh);
  CAF_CHECK_NOT_EQUAL(a1.dt, a2.dt);
  CAF_CHECK_NOT_EQUAL(a1.dt, a2.st);
  CAF_CHECK_NOT_EQUAL(a1.st, a2.wh);
  CAF_CHECK_NOT_EQUAL(a1.st, a2.dt);
  CAF_CHECK_NOT_EQUAL(a1.st, a2.st);
  // all handles in a2 are *not* equal to any handle in a0 or a1
  CAF_CHECK_NOT_EQUAL(a2.wh, a0.wh);
  CAF_CHECK_NOT_EQUAL(a2.wh, a0.dt);
  CAF_CHECK_NOT_EQUAL(a2.wh, a0.st);
  CAF_CHECK_NOT_EQUAL(a2.dt, a0.wh);
  CAF_CHECK_NOT_EQUAL(a2.dt, a0.dt);
  CAF_CHECK_NOT_EQUAL(a2.dt, a0.st);
  CAF_CHECK_NOT_EQUAL(a2.st, a0.wh);
  CAF_CHECK_NOT_EQUAL(a2.st, a0.dt);
  CAF_CHECK_NOT_EQUAL(a2.st, a0.st);
  CAF_CHECK_NOT_EQUAL(a2.wh, a1.wh);
  CAF_CHECK_NOT_EQUAL(a2.wh, a1.dt);
  CAF_CHECK_NOT_EQUAL(a2.wh, a1.st);
  CAF_CHECK_NOT_EQUAL(a2.dt, a1.wh);
  CAF_CHECK_NOT_EQUAL(a2.dt, a1.dt);
  CAF_CHECK_NOT_EQUAL(a2.dt, a1.st);
  CAF_CHECK_NOT_EQUAL(a2.st, a1.wh);
  CAF_CHECK_NOT_EQUAL(a2.st, a1.dt);
  CAF_CHECK_NOT_EQUAL(a2.st, a1.st);
}

CAF_TEST(ordering) {
  // handles in a0 are all equal, i.e., are not in less-than relation
  CAF_CHECK_NOT_LESS(a0.wh, a0.wh);
  CAF_CHECK_NOT_LESS(a0.wh, a0.dt);
  CAF_CHECK_NOT_LESS(a0.wh, a0.st);
  CAF_CHECK_NOT_LESS(a0.dt, a0.wh);
  CAF_CHECK_NOT_LESS(a0.dt, a0.dt);
  CAF_CHECK_NOT_LESS(a0.dt, a0.st);
  CAF_CHECK_NOT_LESS(a0.st, a0.wh);
  CAF_CHECK_NOT_LESS(a0.st, a0.dt);
  CAF_CHECK_NOT_LESS(a0.st, a0.st);
  // handles in a1 are all equal, i.e., are not in less-than relation
  CAF_CHECK_NOT_LESS(a1.wh, a1.wh);
  CAF_CHECK_NOT_LESS(a1.wh, a1.dt);
  CAF_CHECK_NOT_LESS(a1.wh, a1.st);
  CAF_CHECK_NOT_LESS(a1.dt, a1.wh);
  CAF_CHECK_NOT_LESS(a1.dt, a1.dt);
  CAF_CHECK_NOT_LESS(a1.dt, a1.st);
  CAF_CHECK_NOT_LESS(a1.st, a1.wh);
  CAF_CHECK_NOT_LESS(a1.st, a1.dt);
  CAF_CHECK_NOT_LESS(a1.st, a1.st);
  // handles in a2 are all equal, i.e., are not in less-than relation
  CAF_CHECK_NOT_LESS(a2.wh, a2.wh);
  CAF_CHECK_NOT_LESS(a2.wh, a2.dt);
  CAF_CHECK_NOT_LESS(a2.wh, a2.st);
  CAF_CHECK_NOT_LESS(a2.dt, a2.wh);
  CAF_CHECK_NOT_LESS(a2.dt, a2.dt);
  CAF_CHECK_NOT_LESS(a2.dt, a2.st);
  CAF_CHECK_NOT_LESS(a2.st, a2.wh);
  CAF_CHECK_NOT_LESS(a2.st, a2.dt);
  CAF_CHECK_NOT_LESS(a2.st, a2.st);
  // all handles in a0 are less than handles in a1 or a2
  CAF_CHECK_LESS(a0.wh, a1.wh);
  CAF_CHECK_LESS(a0.wh, a1.dt);
  CAF_CHECK_LESS(a0.wh, a1.st);
  CAF_CHECK_LESS(a0.dt, a1.wh);
  CAF_CHECK_LESS(a0.dt, a1.dt);
  CAF_CHECK_LESS(a0.dt, a1.st);
  CAF_CHECK_LESS(a0.st, a1.wh);
  CAF_CHECK_LESS(a0.st, a1.dt);
  CAF_CHECK_LESS(a0.st, a1.st);
  CAF_CHECK_LESS(a0.wh, a2.wh);
  CAF_CHECK_LESS(a0.wh, a2.dt);
  CAF_CHECK_LESS(a0.wh, a2.st);
  CAF_CHECK_LESS(a0.dt, a2.wh);
  CAF_CHECK_LESS(a0.dt, a2.dt);
  CAF_CHECK_LESS(a0.dt, a2.st);
  CAF_CHECK_LESS(a0.st, a2.wh);
  CAF_CHECK_LESS(a0.st, a2.dt);
  CAF_CHECK_LESS(a0.st, a2.st);
  // all handles in a1 are less than handles in a2
  CAF_CHECK_LESS(a1.wh, a2.wh);
  CAF_CHECK_LESS(a1.wh, a2.dt);
  CAF_CHECK_LESS(a1.wh, a2.st);
  CAF_CHECK_LESS(a1.dt, a2.wh);
  CAF_CHECK_LESS(a1.dt, a2.dt);
  CAF_CHECK_LESS(a1.dt, a2.st);
  CAF_CHECK_LESS(a1.st, a2.wh);
  CAF_CHECK_LESS(a1.st, a2.dt);
  CAF_CHECK_LESS(a1.st, a2.st);
  // all handles in a1 are *not* less than handles in a0
  CAF_CHECK_NOT_LESS(a1.wh, a0.wh);
  CAF_CHECK_NOT_LESS(a1.wh, a0.dt);
  CAF_CHECK_NOT_LESS(a1.wh, a0.st);
  CAF_CHECK_NOT_LESS(a1.dt, a0.wh);
  CAF_CHECK_NOT_LESS(a1.dt, a0.dt);
  CAF_CHECK_NOT_LESS(a1.dt, a0.st);
  CAF_CHECK_NOT_LESS(a1.st, a0.wh);
  CAF_CHECK_NOT_LESS(a1.st, a0.dt);
  CAF_CHECK_NOT_LESS(a1.st, a0.st);
  // all handles in a2 are *not* less than handles in a0 or a1
  CAF_CHECK_NOT_LESS(a2.wh, a0.wh);
  CAF_CHECK_NOT_LESS(a2.wh, a0.dt);
  CAF_CHECK_NOT_LESS(a2.wh, a0.st);
  CAF_CHECK_NOT_LESS(a2.dt, a0.wh);
  CAF_CHECK_NOT_LESS(a2.dt, a0.dt);
  CAF_CHECK_NOT_LESS(a2.dt, a0.st);
  CAF_CHECK_NOT_LESS(a2.st, a0.wh);
  CAF_CHECK_NOT_LESS(a2.st, a0.dt);
  CAF_CHECK_NOT_LESS(a2.st, a0.st);
  CAF_CHECK_NOT_LESS(a2.wh, a1.wh);
  CAF_CHECK_NOT_LESS(a2.wh, a1.dt);
  CAF_CHECK_NOT_LESS(a2.wh, a1.st);
  CAF_CHECK_NOT_LESS(a2.dt, a1.wh);
  CAF_CHECK_NOT_LESS(a2.dt, a1.dt);
  CAF_CHECK_NOT_LESS(a2.dt, a1.st);
  CAF_CHECK_NOT_LESS(a2.st, a1.wh);
  CAF_CHECK_NOT_LESS(a2.st, a1.dt);
  CAF_CHECK_NOT_LESS(a2.st, a1.st);
}

CAF_TEST(string_representation) {
  auto s1 = a0.wh;
  auto s2 = a0.dt;
  auto s3 = a0.st;
  CAF_CHECK_EQUAL(s1, s2);
  CAF_CHECK_EQUAL(s2, s3);
}

CAF_TEST(mpi_string_representation) {
  CAF_CHECK(sys.message_types(a0.dt).empty());
  std::set<std::string> st_expected{"caf::replies_to<@i32>::with<@i32>"};
  CAF_CHECK_EQUAL(st_expected, sys.message_types(a0.st));
  CAF_CHECK_EQUAL(st_expected, sys.message_types<testee_actor>());
}

CAF_TEST_FIXTURE_SCOPE_END()

