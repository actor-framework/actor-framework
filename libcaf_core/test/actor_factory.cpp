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

#define CAF_SUITE actor_factory
#include "caf/test/unit_test.hpp"

#include "caf/all.hpp"

#include "caf/actor_registry.hpp"

using namespace caf;

using std::endl;

namespace {

using down_atom = atom_constant<atom("down")>;

struct fixture {
  actor_system_config cfg;

  void test_spawn(message args, bool expect_fail = false) {
    actor_system system{cfg};
    scoped_actor self{system};
    CAF_MESSAGE("set aut");
    strong_actor_ptr res;
    std::set<std::string> ifs;
    scoped_execution_unit context{&system};
    actor_config actor_cfg{&context};
    auto aut = system.spawn<actor>("test_actor", std::move(args));
    if (expect_fail) {
      CAF_REQUIRE(!aut);
      return;
    }
    CAF_REQUIRE(aut);
    self->wait_for(*aut);
    CAF_MESSAGE("aut done");
  }
};

struct test_actor_no_args : event_based_actor {
  using event_based_actor::event_based_actor;
};

struct test_actor_one_arg : event_based_actor {
  test_actor_one_arg(actor_config& conf, int value) : event_based_actor(conf) {
    CAF_CHECK_EQUAL(value, 42);
  }
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(add_actor_type_tests, fixture)

CAF_TEST(fun_no_args) {
  auto test_actor_one_arg = [] {
    CAF_MESSAGE("inside test_actor");
  };
  cfg.add_actor_type("test_actor", test_actor_one_arg);
  test_spawn(make_message());
  CAF_MESSAGE("test_spawn done");
}

CAF_TEST(fun_no_args_selfptr) {
  auto test_actor_one_arg = [](event_based_actor*) {
    CAF_MESSAGE("inside test_actor");
  };
  cfg.add_actor_type("test_actor", test_actor_one_arg);
  test_spawn(make_message());
}
CAF_TEST(fun_one_arg) {
  auto test_actor_one_arg = [](int i) {
    CAF_CHECK_EQUAL(i, 42);
  };
  cfg.add_actor_type("test_actor", test_actor_one_arg);
  test_spawn(make_message(42));
}

CAF_TEST(fun_one_arg_selfptr) {
  auto test_actor_one_arg = [](event_based_actor*, int i) {
    CAF_CHECK_EQUAL(i, 42);
  };
  cfg.add_actor_type("test_actor", test_actor_one_arg);
  test_spawn(make_message(42));
}

CAF_TEST(class_no_arg_invalid) {
  cfg.add_actor_type<test_actor_no_args>("test_actor");
  test_spawn(make_message(42), true);
}

CAF_TEST(class_no_arg_valid) {
  cfg.add_actor_type<test_actor_no_args>("test_actor");
  test_spawn(make_message());
}

CAF_TEST(class_one_arg_invalid) {
  cfg.add_actor_type<test_actor_one_arg, const int&>("test_actor");
  test_spawn(make_message(), true);
}

CAF_TEST(class_one_arg_valid) {
  cfg.add_actor_type<test_actor_one_arg, const int&>("test_actor");
  test_spawn(make_message(42));
}

CAF_TEST_FIXTURE_SCOPE_END()
