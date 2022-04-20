// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE function_view

#include "caf/function_view.hpp"

#include "core-test.hpp"

#include <string>
#include <vector>

using namespace caf;

namespace {

using calculator = typed_actor<replies_to<int, int>::with<int>>;

calculator::behavior_type adder() {
  return {
    [](int x, int y) { return x + y; },
  };
}

calculator::behavior_type multiplier() {
  return {
    [](int x, int y) { return x * y; },
  };
}

calculator::behavior_type divider() {
  return {
    [](int x, int y) -> result<int> {
      if (y == 0)
        return make_error(sec::runtime_error, "division by zero");
      return x / y;
    },
  };
}

using doubler = typed_actor<replies_to<int>::with<int, int>>;

doubler::behavior_type simple_doubler() {
  return {
    [](int x) -> result<int, int> {
      return {x, x};
    },
  };
}

using cell
  = typed_actor<reacts_to<put_atom, int>, replies_to<get_atom>::with<int>>;

struct cell_state {
  int value = 0;
};

cell::behavior_type simple_cell(cell::stateful_pointer<cell_state> self) {
  return {
    [=](put_atom, int val) { self->state.value = val; },
    [=](get_atom) { return self->state.value; },
  };
}

struct fixture {
  fixture() : system(cfg) {
    // nop
  }

  actor_system_config cfg;
  actor_system system;
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

CAF_TEST(empty_function_fiew) {
  function_view<calculator> f;
  CHECK_EQ(f(10, 20), sec::bad_function_call);
}

CAF_TEST(single_res_function_view) {
  auto f = make_function_view(system.spawn(adder));
  CHECK_EQ(f(3, 4), 7);
  CHECK(f != nullptr);
  CHECK(nullptr != f);
  function_view<calculator> g;
  g = std::move(f);
  CHECK(f == nullptr);
  CHECK(nullptr == f);
  CHECK(g != nullptr);
  CHECK(nullptr != g);
  CHECK_EQ(g(10, 20), 30);
  g.assign(system.spawn(multiplier));
  CHECK_EQ(g(10, 20), 200);
  g.assign(system.spawn(divider));
  CHECK(!g(1, 0));
  g.assign(system.spawn(divider));
  CHECK_EQ(g(4, 2), 2);
}

CAF_TEST(tuple_res_function_view) {
  auto f = make_function_view(system.spawn(simple_doubler));
  CHECK_EQ(f(10), std::make_tuple(10, 10));
}

CAF_TEST(cell_function_view) {
  auto f = make_function_view(system.spawn(simple_cell));
  CHECK_EQ(f(get_atom_v), 0);
  f(put_atom_v, 1024);
  CHECK_EQ(f(get_atom_v), 1024);
}

END_FIXTURE_SCOPE()
