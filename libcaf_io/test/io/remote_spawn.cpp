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

#define CAF_SUITE io.remote_spawn

#include "caf/config.hpp"

#include "caf/test/io_dsl.hpp"

#include <cstring>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

using namespace caf;

namespace {

using calculator = typed_actor<replies_to<add_atom, int, int>::with<int>,
                               replies_to<sub_atom, int, int>::with<int>>;

// function-based, dynamically typed, event-based API
behavior calculator_fun(event_based_actor*) {
  return {
    [](add_atom, int a, int b) { return a + b; },
    [](sub_atom, int a, int b) { return a - b; },
  };
}

class calculator_class : public event_based_actor {
public:
  calculator_class(actor_config& cfg) : event_based_actor(cfg) {
    // nop
  }

  behavior make_behavior() override {
    return {
      [](add_atom, int a, int b) { return a + b; },
      [](sub_atom, int a, int b) { return a - b; },
    };
  }
};

// function-based, statically typed, event-based API
calculator::behavior_type typed_calculator_fun() {
  return {
    [](add_atom, int a, int b) { return a + b; },
    [](sub_atom, int a, int b) { return a - b; },
  };
}

struct config : actor_system_config {
  config() {
    load<io::middleman>();
    add_actor_type<calculator_class>("calculator-class");
    add_actor_type("calculator", calculator_fun);
    add_actor_type("typed_calculator", typed_calculator_fun);
  }
};

struct fixture : point_to_point_fixture<test_coordinator_fixture<config>> {
  fixture() {
    prepare_connection(mars, earth, "mars", 8080);
  }
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(dynamic_remote_actor_tests, fixture)

CAF_TEST(nodes can spawn actors remotely) {
  loop_after_next_enqueue(mars);
  CAF_CHECK_EQUAL(unbox(mars.mm.open(8080)), 8080);
  loop_after_next_enqueue(earth);
  auto nid = unbox(earth.mm.connect("mars", 8080));
  CAF_REQUIRE_EQUAL(nid, mars.sys.node());
  CAF_MESSAGE("remote_spawn perform type checks on the handle");
  loop_after_next_enqueue(earth);
  auto calc = earth.mm.remote_spawn<calculator>(nid, "calculator",
                                                make_message());
  CAF_REQUIRE_EQUAL(calc, sec::unexpected_actor_messaging_interface);
  loop_after_next_enqueue(earth);
  calc = earth.mm.remote_spawn<calculator>(nid, "typed_calculator",
                                           make_message());
  CAF_MESSAGE("remotely spawned actors respond to messages");
  earth.self->send(*calc, add_atom_v, 10, 20);
  run();
  expect_on(earth, (int), from(*calc).to(earth.self).with(30));
  earth.self->send(*calc, sub_atom_v, 10, 20);
  run();
  expect_on(earth, (int), from(*calc).to(earth.self).with(-10));
  anon_send_exit(*calc, exit_reason::user_shutdown);
  CAF_MESSAGE("remote_spawn works with class-based actors as well");
  loop_after_next_enqueue(earth);
  auto dyn_calc = earth.mm.remote_spawn<actor>(nid, "calculator-class",
                                               make_message());
  CAF_REQUIRE(dyn_calc);
  earth.self->send(*dyn_calc, add_atom_v, 10, 20);
  run();
  expect_on(earth, (int), from(*dyn_calc).to(earth.self).with(30));
  anon_send_exit(*dyn_calc, exit_reason::user_shutdown);
}

CAF_TEST_FIXTURE_SCOPE_END()
