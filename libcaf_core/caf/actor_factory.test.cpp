// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/test.hpp"

#include "caf/actor_registry.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/all.hpp"
#include "caf/log/test.hpp"
#include "caf/type_id.hpp"

using namespace caf;

using std::endl;

namespace {

struct fixture {
  actor_system_config cfg;

  void test_spawn(message args, bool expect_fail = false) {
    actor_system system{cfg};
    scoped_actor self{system};
    log::test::debug("set aut");
    strong_actor_ptr res;
    std::set<std::string> ifs;
    actor_config actor_cfg{&system.scheduler()};
    auto aut = system.spawn<actor>("test_actor", std::move(args));
    if (expect_fail) {
      test::runnable::current().require(!aut);
      return;
    }
    test::runnable::current().require(static_cast<bool>(aut));
    self->wait_for(*aut);
    log::test::debug("aut done");
  }
};

struct test_actor_no_args : event_based_actor {
  using event_based_actor::event_based_actor;
};

struct test_actor_one_arg : event_based_actor {
  test_actor_one_arg(actor_config& conf, int value) : event_based_actor(conf) {
    test::runnable::current().check_eq(value, 42);
  }
};

WITH_FIXTURE(fixture) {

TEST("fun_no_args") {
  auto test_actor_one_arg = [] { log::test::debug("inside test_actor"); };
  cfg.add_actor_type("test_actor", test_actor_one_arg);
  test_spawn(make_message());
  log::test::debug("test_spawn done");
}

TEST("fun_no_args_selfptr") {
  auto test_actor_one_arg = [](event_based_actor*) {
    log::test::debug("inside test_actor");
  };
  cfg.add_actor_type("test_actor", test_actor_one_arg);
  test_spawn(make_message());
}
TEST("fun_one_arg") {
  auto test_actor_one_arg = [this](int i) { check_eq(i, 42); };
  cfg.add_actor_type("test_actor", test_actor_one_arg);
  test_spawn(make_message(42));
}

TEST("fun_one_arg_selfptr") {
  auto test_actor_one_arg = [this](event_based_actor*, int i) {
    check_eq(i, 42);
  };
  cfg.add_actor_type("test_actor", test_actor_one_arg);
  test_spawn(make_message(42));
}

TEST("class_no_arg_invalid") {
  cfg.add_actor_type<test_actor_no_args>("test_actor");
  test_spawn(make_message(42), true);
}

TEST("class_no_arg_valid") {
  cfg.add_actor_type<test_actor_no_args>("test_actor");
  test_spawn(make_message());
}

TEST("class_one_arg_invalid") {
  cfg.add_actor_type<test_actor_one_arg, const int&>("test_actor");
  test_spawn(make_message(), true);
}

TEST("class_one_arg_valid") {
  cfg.add_actor_type<test_actor_one_arg, const int&>("test_actor");
  test_spawn(make_message(42));
}

} // WITH_FIXTURE(fixture)

} // namespace
