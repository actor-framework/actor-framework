// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/function_view.hpp"

#include "caf/test/test.hpp"

#include "caf/actor_registry.hpp"
#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/typed_event_based_actor.hpp"

#include <string>
#include <vector>

using namespace caf;
using namespace std::literals;

namespace {

using calculator = typed_actor<result<int>(int, int)>;

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

using doubler = typed_actor<result<int, int>(int)>;

doubler::behavior_type simple_doubler() {
  return {
    [](int x) -> result<int, int> { return {x, x}; },
  };
}

using cell = typed_actor<result<void>(put_atom, int), result<int>(get_atom)>;

struct cell_state {
  int value = 0;
};

cell::behavior_type simple_cell(cell::stateful_pointer<cell_state> self) {
  return {
    [=](put_atom, int val) { self->state().value = val; },
    [=](get_atom) { return self->state().value; },
  };
}

struct fixture {
  fixture() : system(cfg) {
    // nop
  }

  actor_system_config cfg;
  actor_system system;
};

WITH_FIXTURE(fixture) {

TEST("empty_function_view") {
  function_view<calculator> f;
  check_eq(f(10, 20), sec::bad_function_call);
}

TEST("single_res_function_view") {
  auto f = make_function_view(system.spawn(adder));
  check_eq(f(3, 4), 7);
  check(f != nullptr);
  check(nullptr != f);
  function_view<calculator> g;
  g = std::move(f);
  check(f == nullptr); // NOLINT(bugprone-use-after-move)
  check(nullptr == f);
  check(g != nullptr);
  check(nullptr != g);
  check_eq(g(10, 20), 30);
  g.assign(system.spawn(multiplier));
  check_eq(g(10, 20), 200);
  g.assign(system.spawn(divider));
  check(!g(1, 0));
  g.assign(system.spawn(divider));
  check_eq(g(4, 2), 2);
}

TEST("tuple_res_function_view") {
  auto f = make_function_view(system.spawn(simple_doubler));
  check_eq(f(10), std::make_tuple(10, 10));
}

TEST("cell_function_view") {
  auto f = make_function_view(system.spawn(simple_cell));
  check_eq(f(get_atom_v), 0);
  f(put_atom_v, 1024);
  check_eq(f(get_atom_v), 1024);
}

TEST("calling function_view on an actor that quit") {
  auto simple = system.spawn(simple_cell);
  auto f = make_function_view(simple);
  anon_send_exit(simple, exit_reason::user_shutdown);
  system.registry().await_running_count_equal(1);
  check_ne(f(get_atom_v), error{});
}

} // WITH_FIXTURE(fixture)

} // namespace
