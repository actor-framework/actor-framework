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

#define CAF_SUITE function_view
#include "caf/test/unit_test.hpp"

#include <string>
#include <vector>

#include "caf/all.hpp"

using namespace caf;

namespace {

using calculator = typed_actor<replies_to<int, int>::with<int>>;

calculator::behavior_type adder() {
  return {
    [](int x, int y) {
      return x + y;
    }
  };
}

calculator::behavior_type multiplier() {
  return {
    [](int x, int y) {
      return x * y;
    }
  };
}

calculator::behavior_type divider() {
  return {
    [](int x, int y) -> optional<int> {
      if (y == 0)
        return none;
      return x / y;
    }
  };
}

using doubler = typed_actor<replies_to<int>::with<int, int>>;

doubler::behavior_type simple_doubler() {
  return {
    [](int x) {
      return std::make_tuple(x, x);
    }
  };
}

using cell = typed_actor<reacts_to<put_atom, int>,
                         replies_to<get_atom>::with<int>>;

struct cell_state {
  int value = 0;
};

cell::behavior_type
simple_cell(cell::stateful_pointer<cell_state> self) {
  return {
    [=](put_atom, int val) {
      self->state.value = val;
    },
    [=](get_atom) {
      return self->state.value;
    }
  };
}

struct fixture {
  fixture() : system(cfg) {
    // nop
  }

  actor_system_config cfg;
  actor_system system;
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(function_view_tests, fixture)

CAF_TEST(empty_function_fiew) {
  function_view<calculator> f;
  CAF_CHECK_EQUAL(f(10, 20), sec::bad_function_call);
}

CAF_TEST(single_res_function_view) {
  auto f = make_function_view(system.spawn(adder));
  CAF_CHECK_EQUAL(f(3, 4), 7);
  CAF_CHECK(f != nullptr);
  CAF_CHECK(nullptr != f);
  function_view<calculator> g;
  g = std::move(f);
  CAF_CHECK(f == nullptr);
  CAF_CHECK(nullptr == f);
  CAF_CHECK(g != nullptr);
  CAF_CHECK(nullptr != g);
  CAF_CHECK_EQUAL(g(10, 20), 30);
  g.assign(system.spawn(multiplier));
  CAF_CHECK_EQUAL(g(10, 20), 200);
  g.assign(system.spawn(divider));
  CAF_CHECK(!g(1, 0));
  g.assign(system.spawn(divider));
  CAF_CHECK_EQUAL(g(4, 2), 2);
}

CAF_TEST(tuple_res_function_view) {
  auto f = make_function_view(system.spawn(simple_doubler));
  CAF_CHECK_EQUAL(f(10), std::make_tuple(10, 10));
}

CAF_TEST(cell_function_view) {
  auto f = make_function_view(system.spawn(simple_cell));
  CAF_CHECK_EQUAL(f(get_atom::value), 0);
  f(put_atom::value, 1024);
  CAF_CHECK_EQUAL(f(get_atom::value), 1024);
}

CAF_TEST_FIXTURE_SCOPE_END()
