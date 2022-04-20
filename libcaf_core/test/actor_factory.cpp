// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE actor_factory

#include "caf/actor_system_config.hpp"

#include "core-test.hpp"

#include "caf/actor_registry.hpp"
#include "caf/all.hpp"
#include "caf/type_id.hpp"

using namespace caf;

using std::endl;

namespace {

struct fixture {
  actor_system_config cfg;

  void test_spawn(message args, bool expect_fail = false) {
    actor_system system{cfg};
    scoped_actor self{system};
    MESSAGE("set aut");
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
    MESSAGE("aut done");
  }
};

struct test_actor_no_args : event_based_actor {
  using event_based_actor::event_based_actor;
};

struct test_actor_one_arg : event_based_actor {
  test_actor_one_arg(actor_config& conf, int value) : event_based_actor(conf) {
    CHECK_EQ(value, 42);
  }
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

CAF_TEST(fun_no_args) {
  auto test_actor_one_arg = [] { MESSAGE("inside test_actor"); };
  cfg.add_actor_type("test_actor", test_actor_one_arg);
  test_spawn(make_message());
  MESSAGE("test_spawn done");
}

CAF_TEST(fun_no_args_selfptr) {
  auto test_actor_one_arg = [](event_based_actor*) {
    MESSAGE("inside test_actor");
  };
  cfg.add_actor_type("test_actor", test_actor_one_arg);
  test_spawn(make_message());
}
CAF_TEST(fun_one_arg) {
  auto test_actor_one_arg = [](int i) { CHECK_EQ(i, 42); };
  cfg.add_actor_type("test_actor", test_actor_one_arg);
  test_spawn(make_message(42));
}

CAF_TEST(fun_one_arg_selfptr) {
  auto test_actor_one_arg = [](event_based_actor*, int i) { CHECK_EQ(i, 42); };
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

END_FIXTURE_SCOPE()
