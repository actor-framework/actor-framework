// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/logger.hpp"

#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/log/level.hpp"
#include "caf/scoped_actor.hpp"

using namespace caf;
using namespace std::literals;

TEST("line_builder appends strings with space separators") {
  SECTION("string_view") {
    auto result = (logger::line_builder{} << "hello"sv << "world"sv).get();
    check_eq(result, "hello world");
  }
  SECTION("const char*") {
    auto result = (logger::line_builder{} << "foo" << "bar").get();
    check_eq(result, "foo bar");
  }
  SECTION("std::string") {
    auto result = (logger::line_builder{} << std::string{"a"} << "b").get();
    check_eq(result, "a b");
  }
  SECTION("char") {
    auto result = (logger::line_builder{} << 'x' << 'y').get();
    check_eq(result, "x y");
  }
  SECTION("mixed types") {
    auto result = (logger::line_builder{} << "msg" << 42 << "end").get();
    check_eq(result, "msg 42 end");
  }
}

TEST("logger make returns a non-null intrusive_ptr") {
  actor_system_config cfg;
  actor_system sys{cfg};
  auto lg = logger::make(sys);
  check_ne(lg, nullptr);
}

TEST("current_logger get and set") {
  SECTION("can be set to null") {
    logger::current_logger(nullptr);
    check_eq(logger::current_logger(), nullptr);
  }
  SECTION("is automatically set when creating an actor system") {
    actor_system_config cfg;
    actor_system sys{cfg};
    check_ne(logger::current_logger(), nullptr);
  }
  SECTION("can be manually set to a specific actor system") {
    actor_system_config cfg;
    actor_system sys{cfg};
    logger::current_logger(nullptr);
    check_eq(logger::current_logger(), nullptr);
    logger::current_logger(&sys);
    check_eq(logger::current_logger(), &sys.logger());
  }
  SECTION("is null after actor system is destroyed") {
    {
      actor_system_config cfg;
      actor_system sys{cfg};
    }
    check_eq(logger::current_logger(), nullptr);
  }
}

TEST("thread_local_aid returns the current actor ID") {
  SECTION("returns zero when no current actor") {
    check_eq(logger::thread_local_aid(), 0u);
  }
  SECTION("returns scoped_actor ID when scoped_actor is active") {
    actor_system_config cfg;
    actor_system sys{cfg};
    scoped_actor self{sys};
    check_eq(logger::thread_local_aid(), self->id());
  }
}

SCENARIO("logger accepts returns false for levels above verbosity") {
  GIVEN("an actor system with warning verbosity") {
    actor_system_config cfg;
    cfg.set("caf.logger.console.verbosity", "warning");
    actor_system sys{cfg};
    WHEN("checking accepts for each level") {
      THEN("accepts is true for error and warning, false for info and below") {
        check(sys.logger().accepts(log::level::error, "test"));
        check(sys.logger().accepts(log::level::warning, "test"));
        check(!sys.logger().accepts(log::level::info, "test"));
        check(!sys.logger().accepts(log::level::debug, "test"));
        check(!sys.logger().accepts(log::level::trace, "test"));
      }
    }
  }
}
