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

#define CAF_SUITE io_remote_spawn
#include "caf/test/dsl.hpp"

#include <thread>
#include <string>
#include <cstring>
#include <sstream>
#include <iostream>
#include <functional>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

using namespace caf;

namespace {

using add_atom = atom_constant<atom("add")>;
using sub_atom = atom_constant<atom("sub")>;

using calculator = typed_actor<replies_to<add_atom, int, int>::with<int>,
                               replies_to<sub_atom, int, int>::with<int>>;

// function-based, dynamically typed, event-based API
behavior calculator_fun(event_based_actor*) {
  return behavior{
    [](add_atom, int a, int b) {
      return a + b;
    },
    [](sub_atom, int a, int b) {
      return a - b;
    }
  };
}

// function-based, statically typed, event-based API
calculator::behavior_type typed_calculator_fun() {
  return {
    [](add_atom, int a, int b) {
      return a + b;
    },
    [](sub_atom, int a, int b) {
      return a - b;
    }
  };
}

struct config : actor_system_config {
  config(int argc, char** argv) {
    parse(argc, argv);
    load<io::middleman>();
    add_actor_type("calculator", calculator_fun);
    add_actor_type("typed_calculator", typed_calculator_fun);
  }
};

void run_client(int argc, char** argv, uint16_t port) {
  config cfg{argc, argv};
  actor_system system{cfg};
  auto& mm = system.middleman();
  auto nid = mm.connect("localhost", port);
  CAF_REQUIRE(nid);
  CAF_REQUIRE_NOT_EQUAL(system.node(), *nid);
  auto calc = mm.remote_spawn<calculator>(*nid, "calculator", make_message());
  CAF_REQUIRE(!calc);
  CAF_REQUIRE_EQUAL(calc.error().category(), atom("system"));
  CAF_REQUIRE_EQUAL(static_cast<sec>(calc.error().code()),
                    sec::unexpected_actor_messaging_interface);
  calc = mm.remote_spawn<calculator>(*nid, "typed_calculator", make_message());
  CAF_REQUIRE(calc);
  auto f1 = make_function_view(*calc);
  CAF_REQUIRE_EQUAL(f1(add_atom::value, 10, 20), 30);
  CAF_REQUIRE_EQUAL(f1(sub_atom::value, 10, 20), -10);
  f1.reset();
  anon_send_exit(*calc, exit_reason::kill);
  mm.close(port);
}

void run_server(int argc, char** argv) {
  config cfg{argc, argv};
  actor_system system{cfg};
  auto port = unbox(system.middleman().open(0));
  std::thread child{[=] { run_client(argc, argv, port); }};
  child.join();
}

} // namespace <anonymous>

CAF_TEST(remote_spawn) {
  auto argc = test::engine::argc();
  auto argv = test::engine::argv();
  run_server(argc, argv);
}
