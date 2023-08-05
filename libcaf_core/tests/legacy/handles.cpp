// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

// This unit test checks guarantees regarding ordering and equality for actor
// handles, i.e., actor_addr, actor, and typed_actor<...>.

#define CAF_SUITE handles

#include "caf/actor.hpp"
#include "caf/actor_addr.hpp"
#include "caf/typed_actor.hpp"

#include "core-test.hpp"

using namespace caf;

namespace {

// Simple int32 interface for testee actors.
using testee_actor = typed_actor<result<int32_t>(int32_t)>;

// Dynamically typed testee.
behavior dt_testee() {
  return {[](int32_t x) { return x * x; }};
}

// Statically typed testee.
testee_actor::behavior_type st_testee() {
  return {[](int32_t x) { return x * x; }};
}

// A simple wrapper for storing a handle in all representations.
struct handle_set {
  // Weak handle to the actor.
  actor_addr wh;
  // Dynamically typed handle to the actor.
  actor dt;
  // Statically typed handle to the actor.
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

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

CAF_TEST(identity) {
  // all handles in a0 are equal
  CHECK_EQ(a0.wh, a0.wh);
  CHECK_EQ(a0.wh, a0.dt);
  CHECK_EQ(a0.wh, a0.st);
  CHECK_EQ(a0.dt, a0.wh);
  CHECK_EQ(a0.dt, a0.dt);
  CHECK_EQ(a0.dt, a0.st);
  CHECK_EQ(a0.st, a0.wh);
  CHECK_EQ(a0.st, a0.dt);
  CHECK_EQ(a0.st, a0.st);
  // all handles in a1 are equal
  CHECK_EQ(a1.wh, a1.wh);
  CHECK_EQ(a1.wh, a1.dt);
  CHECK_EQ(a1.wh, a1.st);
  CHECK_EQ(a1.dt, a1.wh);
  CHECK_EQ(a1.dt, a1.dt);
  CHECK_EQ(a1.dt, a1.st);
  CHECK_EQ(a1.st, a1.wh);
  CHECK_EQ(a1.st, a1.dt);
  CHECK_EQ(a1.st, a1.st);
  // all handles in a2 are equal
  CHECK_EQ(a2.wh, a2.wh);
  CHECK_EQ(a2.wh, a2.dt);
  CHECK_EQ(a2.wh, a2.st);
  CHECK_EQ(a2.dt, a2.wh);
  CHECK_EQ(a2.dt, a2.dt);
  CHECK_EQ(a2.dt, a2.st);
  CHECK_EQ(a2.st, a2.wh);
  CHECK_EQ(a2.st, a2.dt);
  CHECK_EQ(a2.st, a2.st);
  // all handles in a0 are *not* equal to any handle in a1 or a2
  CHECK_NE(a0.wh, a1.wh);
  CHECK_NE(a0.wh, a1.dt);
  CHECK_NE(a0.wh, a1.st);
  CHECK_NE(a0.dt, a1.wh);
  CHECK_NE(a0.dt, a1.dt);
  CHECK_NE(a0.dt, a1.st);
  CHECK_NE(a0.st, a1.wh);
  CHECK_NE(a0.st, a1.dt);
  CHECK_NE(a0.st, a1.st);
  CHECK_NE(a0.wh, a2.wh);
  CHECK_NE(a0.wh, a2.dt);
  CHECK_NE(a0.wh, a2.st);
  CHECK_NE(a0.dt, a2.wh);
  CHECK_NE(a0.dt, a2.dt);
  CHECK_NE(a0.dt, a2.st);
  CHECK_NE(a0.st, a2.wh);
  CHECK_NE(a0.st, a2.dt);
  CHECK_NE(a0.st, a2.st);
  // all handles in a1 are *not* equal to any handle in a0 or a2
  CHECK_NE(a1.wh, a0.wh);
  CHECK_NE(a1.wh, a0.dt);
  CHECK_NE(a1.wh, a0.st);
  CHECK_NE(a1.dt, a0.wh);
  CHECK_NE(a1.dt, a0.dt);
  CHECK_NE(a1.dt, a0.st);
  CHECK_NE(a1.st, a0.wh);
  CHECK_NE(a1.st, a0.dt);
  CHECK_NE(a1.st, a0.st);
  CHECK_NE(a1.wh, a2.wh);
  CHECK_NE(a1.wh, a2.dt);
  CHECK_NE(a1.wh, a2.st);
  CHECK_NE(a1.dt, a2.wh);
  CHECK_NE(a1.dt, a2.dt);
  CHECK_NE(a1.dt, a2.st);
  CHECK_NE(a1.st, a2.wh);
  CHECK_NE(a1.st, a2.dt);
  CHECK_NE(a1.st, a2.st);
  // all handles in a2 are *not* equal to any handle in a0 or a1
  CHECK_NE(a2.wh, a0.wh);
  CHECK_NE(a2.wh, a0.dt);
  CHECK_NE(a2.wh, a0.st);
  CHECK_NE(a2.dt, a0.wh);
  CHECK_NE(a2.dt, a0.dt);
  CHECK_NE(a2.dt, a0.st);
  CHECK_NE(a2.st, a0.wh);
  CHECK_NE(a2.st, a0.dt);
  CHECK_NE(a2.st, a0.st);
  CHECK_NE(a2.wh, a1.wh);
  CHECK_NE(a2.wh, a1.dt);
  CHECK_NE(a2.wh, a1.st);
  CHECK_NE(a2.dt, a1.wh);
  CHECK_NE(a2.dt, a1.dt);
  CHECK_NE(a2.dt, a1.st);
  CHECK_NE(a2.st, a1.wh);
  CHECK_NE(a2.st, a1.dt);
  CHECK_NE(a2.st, a1.st);
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
  CHECK_LT(a0.wh, a1.wh);
  CHECK_LT(a0.wh, a1.dt);
  CHECK_LT(a0.wh, a1.st);
  CHECK_LT(a0.dt, a1.wh);
  CHECK_LT(a0.dt, a1.dt);
  CHECK_LT(a0.dt, a1.st);
  CHECK_LT(a0.st, a1.wh);
  CHECK_LT(a0.st, a1.dt);
  CHECK_LT(a0.st, a1.st);
  CHECK_LT(a0.wh, a2.wh);
  CHECK_LT(a0.wh, a2.dt);
  CHECK_LT(a0.wh, a2.st);
  CHECK_LT(a0.dt, a2.wh);
  CHECK_LT(a0.dt, a2.dt);
  CHECK_LT(a0.dt, a2.st);
  CHECK_LT(a0.st, a2.wh);
  CHECK_LT(a0.st, a2.dt);
  CHECK_LT(a0.st, a2.st);
  // all handles in a1 are less than handles in a2
  CHECK_LT(a1.wh, a2.wh);
  CHECK_LT(a1.wh, a2.dt);
  CHECK_LT(a1.wh, a2.st);
  CHECK_LT(a1.dt, a2.wh);
  CHECK_LT(a1.dt, a2.dt);
  CHECK_LT(a1.dt, a2.st);
  CHECK_LT(a1.st, a2.wh);
  CHECK_LT(a1.st, a2.dt);
  CHECK_LT(a1.st, a2.st);
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
  CHECK_EQ(s1, s2);
  CHECK_EQ(s2, s3);
}

CAF_TEST(mpi_string_representation) {
  CHECK(sys.message_types(a0.dt).empty());
  std::set<std::string> st_expected{"(int32_t) -> (int32_t)"};
  CHECK_EQ(st_expected, sys.message_types(a0.st));
  CHECK_EQ(st_expected, sys.message_types<testee_actor>());
}

END_FIXTURE_SCOPE()
