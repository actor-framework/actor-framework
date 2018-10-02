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

#define CAF_SUITE actor_factory

#include "caf/actor_registry.hpp"

#include "caf/test/dsl.hpp"

#include "caf/all.hpp"

using namespace caf;

using std::endl;

namespace {

behavior no_args_1() {
  return {};
}

behavior no_args_2(event_based_actor*) {
  return {};
}

struct no_args_3 : event_based_actor {
  using super = event_based_actor;

  using super::super;
};

behavior one_arg_1(int value) {
  CAF_CHECK_EQUAL(value, 42);
  return {};
}

behavior one_arg_2(event_based_actor*, int value) {
  CAF_CHECK_EQUAL(value, 42);
  return {};
}

struct one_arg_3 : event_based_actor {
  using super = event_based_actor;

  one_arg_3(actor_config& conf, int value) : super(conf) {
    CAF_CHECK_EQUAL(value, 42);
  }
};

struct config : actor_system_config {
  config() {
    add_actor_type("no_args_1", no_args_1);
    add_actor_type("no_args_2", no_args_2);
    add_actor_type<no_args_3>("no_args_3");
    add_actor_type("one_arg_1", one_arg_1);
    add_actor_type("one_arg_2", one_arg_2);
    add_actor_type<one_arg_3, const int&>("one_arg_3");
  }
};

struct fixture : test_coordinator_fixture<config> {
  template <class... Ts>
  expected<actor> test_spawn(const char* name, Ts&&... xs) {
    CAF_MESSAGE("spawn testee of type " << name);
    actor_config actor_cfg{sys.dummy_execution_unit()};
    return sys.spawn<actor>(name, make_message(std::forward<Ts>(xs)...));
  }

  error invalid_args = sec::cannot_spawn_actor_from_arguments;
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(add_actor_type_tests, fixture)

CAF_TEST(no_args) {
  CAF_CHECK_NOT_EQUAL(test_spawn("no_args_1"), none);
  CAF_CHECK_NOT_EQUAL(test_spawn("no_args_2"), none);
  CAF_CHECK_NOT_EQUAL(test_spawn("no_args_3"), none);
  CAF_CHECK_EQUAL(test_spawn("no_args_1", 42), invalid_args);
  CAF_CHECK_EQUAL(test_spawn("no_args_2", 42), invalid_args);
  CAF_CHECK_EQUAL(test_spawn("no_args_3", 42), invalid_args);
}

CAF_TEST(one_arg) {
  CAF_CHECK_NOT_EQUAL(test_spawn("one_arg_1", 42), none);
  CAF_CHECK_NOT_EQUAL(test_spawn("one_arg_2", 42), none);
  CAF_CHECK_NOT_EQUAL(test_spawn("one_arg_3", 42), none);
  CAF_CHECK_EQUAL(test_spawn("one_arg_1"), invalid_args);
  CAF_CHECK_EQUAL(test_spawn("one_arg_2"), invalid_args);
  CAF_CHECK_EQUAL(test_spawn("one_arg_3"), invalid_args);
}

CAF_TEST_FIXTURE_SCOPE_END()
