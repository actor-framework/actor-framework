// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

// This unit test checks guarantees regarding ordering and equality for actor
// handles, i.e., actor_addr, actor, and typed_actor<...>.

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/actor.hpp"
#include "caf/actor_addr.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/typed_actor.hpp"
#include "caf/typed_event_based_actor.hpp"

using namespace caf;

namespace {

// Simple int32 interface for testee actors.
using testee_actor = typed_actor<result<int32_t>(int32_t)>;

// Dynamically typed testee.
behavior dt_testee() {
  return {
    [](int32_t x) { return x * x; },
  };
}

// Statically typed testee.
testee_actor::behavior_type st_testee() {
  return {
    [](int32_t x) { return x * x; },
  };
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

struct fixture : test::fixture::deterministic {
  scoped_actor self{sys};
  handle_set a0;
  handle_set a1{sys.spawn(dt_testee)};
  handle_set a2{sys.spawn(st_testee)};
};

} // namespace

WITH_FIXTURE(fixture) {

TEST("identity") {
  // all handles in a0 are equal
  check_eq(a0.wh, a0.wh);
  check_eq(a0.wh, a0.dt);
  check_eq(a0.wh, a0.st);
  check_eq(a0.dt, a0.wh);
  check_eq(a0.dt, a0.dt);
  check_eq(a0.dt, a0.st);
  check_eq(a0.st, a0.wh);
  check_eq(a0.st, a0.dt);
  check_eq(a0.st, a0.st);
  // all handles in a1 are equal
  check_eq(a1.wh, a1.wh);
  check_eq(a1.wh, a1.dt);
  check_eq(a1.wh, a1.st);
  check_eq(a1.dt, a1.wh);
  check_eq(a1.dt, a1.dt);
  check_eq(a1.dt, a1.st);
  check_eq(a1.st, a1.wh);
  check_eq(a1.st, a1.dt);
  check_eq(a1.st, a1.st);
  // all handles in a2 are equal
  check_eq(a2.wh, a2.wh);
  check_eq(a2.wh, a2.dt);
  check_eq(a2.wh, a2.st);
  check_eq(a2.dt, a2.wh);
  check_eq(a2.dt, a2.dt);
  check_eq(a2.dt, a2.st);
  check_eq(a2.st, a2.wh);
  check_eq(a2.st, a2.dt);
  check_eq(a2.st, a2.st);
  // all handles in a0 are *not* equal to any handle in a1 or a2
  check_ne(a0.wh, a1.wh);
  check_ne(a0.wh, a1.dt);
  check_ne(a0.wh, a1.st);
  check_ne(a0.dt, a1.wh);
  check_ne(a0.dt, a1.dt);
  check_ne(a0.dt, a1.st);
  check_ne(a0.st, a1.wh);
  check_ne(a0.st, a1.dt);
  check_ne(a0.st, a1.st);
  check_ne(a0.wh, a2.wh);
  check_ne(a0.wh, a2.dt);
  check_ne(a0.wh, a2.st);
  check_ne(a0.dt, a2.wh);
  check_ne(a0.dt, a2.dt);
  check_ne(a0.dt, a2.st);
  check_ne(a0.st, a2.wh);
  check_ne(a0.st, a2.dt);
  check_ne(a0.st, a2.st);
  // all handles in a1 are *not* equal to any handle in a0 or a2
  check_ne(a1.wh, a0.wh);
  check_ne(a1.wh, a0.dt);
  check_ne(a1.wh, a0.st);
  check_ne(a1.dt, a0.wh);
  check_ne(a1.dt, a0.dt);
  check_ne(a1.dt, a0.st);
  check_ne(a1.st, a0.wh);
  check_ne(a1.st, a0.dt);
  check_ne(a1.st, a0.st);
  check_ne(a1.wh, a2.wh);
  check_ne(a1.wh, a2.dt);
  check_ne(a1.wh, a2.st);
  check_ne(a1.dt, a2.wh);
  check_ne(a1.dt, a2.dt);
  check_ne(a1.dt, a2.st);
  check_ne(a1.st, a2.wh);
  check_ne(a1.st, a2.dt);
  check_ne(a1.st, a2.st);
  // all handles in a2 are *not* equal to any handle in a0 or a1
  check_ne(a2.wh, a0.wh);
  check_ne(a2.wh, a0.dt);
  check_ne(a2.wh, a0.st);
  check_ne(a2.dt, a0.wh);
  check_ne(a2.dt, a0.dt);
  check_ne(a2.dt, a0.st);
  check_ne(a2.st, a0.wh);
  check_ne(a2.st, a0.dt);
  check_ne(a2.st, a0.st);
  check_ne(a2.wh, a1.wh);
  check_ne(a2.wh, a1.dt);
  check_ne(a2.wh, a1.st);
  check_ne(a2.dt, a1.wh);
  check_ne(a2.dt, a1.dt);
  check_ne(a2.dt, a1.st);
  check_ne(a2.st, a1.wh);
  check_ne(a2.st, a1.dt);
  check_ne(a2.st, a1.st);
}

TEST("ordering") {
  // handles in a0 are all equal, i.e., are not in less-than relation
  check_ge(a0.wh, a0.wh);
  check_ge(a0.wh, a0.dt);
  check_ge(a0.wh, a0.st);
  check_ge(a0.dt, a0.wh);
  check_ge(a0.dt, a0.dt);
  check_ge(a0.dt, a0.st);
  check_ge(a0.st, a0.wh);
  check_ge(a0.st, a0.dt);
  check_ge(a0.st, a0.st);
  // handles in a1 are all equal, i.e., are not in less-than relation
  check_ge(a1.wh, a1.wh);
  check_ge(a1.wh, a1.dt);
  check_ge(a1.wh, a1.st);
  check_ge(a1.dt, a1.wh);
  check_ge(a1.dt, a1.dt);
  check_ge(a1.dt, a1.st);
  check_ge(a1.st, a1.wh);
  check_ge(a1.st, a1.dt);
  check_ge(a1.st, a1.st);
  // handles in a2 are all equal, i.e., are not in less-than relation
  check_ge(a2.wh, a2.wh);
  check_ge(a2.wh, a2.dt);
  check_ge(a2.wh, a2.st);
  check_ge(a2.dt, a2.wh);
  check_ge(a2.dt, a2.dt);
  check_ge(a2.dt, a2.st);
  check_ge(a2.st, a2.wh);
  check_ge(a2.st, a2.dt);
  check_ge(a2.st, a2.st);
  // all handles in a0 are less than handles in a1 or a2
  check_lt(a0.wh, a1.wh);
  check_lt(a0.wh, a1.dt);
  check_lt(a0.wh, a1.st);
  check_lt(a0.dt, a1.wh);
  check_lt(a0.dt, a1.dt);
  check_lt(a0.dt, a1.st);
  check_lt(a0.st, a1.wh);
  check_lt(a0.st, a1.dt);
  check_lt(a0.st, a1.st);
  check_lt(a0.wh, a2.wh);
  check_lt(a0.wh, a2.dt);
  check_lt(a0.wh, a2.st);
  check_lt(a0.dt, a2.wh);
  check_lt(a0.dt, a2.dt);
  check_lt(a0.dt, a2.st);
  check_lt(a0.st, a2.wh);
  check_lt(a0.st, a2.dt);
  check_lt(a0.st, a2.st);
  // all handles in a1 are less than handles in a2
  check_lt(a1.wh, a2.wh);
  check_lt(a1.wh, a2.dt);
  check_lt(a1.wh, a2.st);
  check_lt(a1.dt, a2.wh);
  check_lt(a1.dt, a2.dt);
  check_lt(a1.dt, a2.st);
  check_lt(a1.st, a2.wh);
  check_lt(a1.st, a2.dt);
  check_lt(a1.st, a2.st);
  // all handles in a1 are *not* less than handles in a0
  check_ge(a1.wh, a0.wh);
  check_ge(a1.wh, a0.dt);
  check_ge(a1.wh, a0.st);
  check_ge(a1.dt, a0.wh);
  check_ge(a1.dt, a0.dt);
  check_ge(a1.dt, a0.st);
  check_ge(a1.st, a0.wh);
  check_ge(a1.st, a0.dt);
  check_ge(a1.st, a0.st);
  // all handles in a2 are *not* less than handles in a0 or a1
  check_ge(a2.wh, a0.wh);
  check_ge(a2.wh, a0.dt);
  check_ge(a2.wh, a0.st);
  check_ge(a2.dt, a0.wh);
  check_ge(a2.dt, a0.dt);
  check_ge(a2.dt, a0.st);
  check_ge(a2.st, a0.wh);
  check_ge(a2.st, a0.dt);
  check_ge(a2.st, a0.st);
  check_ge(a2.wh, a1.wh);
  check_ge(a2.wh, a1.dt);
  check_ge(a2.wh, a1.st);
  check_ge(a2.dt, a1.wh);
  check_ge(a2.dt, a1.dt);
  check_ge(a2.dt, a1.st);
  check_ge(a2.st, a1.wh);
  check_ge(a2.st, a1.dt);
  check_ge(a2.st, a1.st);
}

TEST("string_representation") {
  auto s1 = a0.wh;
  auto s2 = a0.dt;
  auto s3 = a0.st;
  check_eq(s1, s2);
  check_eq(s2, s3);
}

TEST("mpi_string_representation") {
  check(sys.message_types(a0.dt).empty());
  std::set<std::string> st_expected{"(int32_t) -> (int32_t)"};
  check_eq(st_expected, sys.message_types(a0.st));
  check_eq(st_expected, sys.message_types<testee_actor>());
}

} // WITH_FIXTURE(fixture)
