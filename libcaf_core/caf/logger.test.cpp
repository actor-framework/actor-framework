// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/logger.hpp"

#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/anon_mail.hpp"
#include "caf/detail/asynchronous_logger.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/log/level.hpp"
#include "caf/scoped_actor.hpp"

#include <filesystem>
#include <future>

using namespace caf;
using namespace std::literals;

namespace {

struct mock_console_printer : public console_printer {
  void print(term color, const char* buf, size_t len) override {
    if (color <= term::reset_endl) {
      lines.emplace_back(buf, len);
    } else {
      auto color_name = to_string(color);
      lines.push_back(detail::format("<{}>{}</{}>\n", color_name,
                                     std::string_view{buf, len}, color_name));
    }
  }

  std::vector<std::string> lines;
};

std::unique_ptr<console_printer> make_mock_console_printer() {
  return std::make_unique<mock_console_printer>();
}

struct test_config : public actor_system_config {
  test_config() {
    console_printer_factory(make_mock_console_printer);
    set("caf.logger.console.stream", "system");
    set("caf.logger.file.path", ".caf-logger-test/output.log");
  }
};

struct fixture {
  fixture() : temp_dir(std::filesystem::current_path() / ".caf-logger-test") {
    std::error_code err;
    std::filesystem::create_directories(temp_dir, err);
    if (err) {
      auto msg = detail::format("failed to create directory '{}': {}\n",
                                temp_dir.string(), err.message());
      fputs(msg.c_str(), stderr);
    }
    cfg.set("caf.logger.file.path", (temp_dir / "output.log").string());
  }

  ~fixture() {
    std::error_code err;
    std::filesystem::remove_all(temp_dir, err);
    if (err) {
      auto msg = detail::format("failed to remove directory '{}': {}\n",
                                temp_dir.string(), err.message());
      fputs(msg.c_str(), stderr);
    }
  }

  test_config cfg;
  std::filesystem::path temp_dir;
};

} // namespace

WITH_FIXTURE(fixture) {

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

TEST("asynchronous_logger::make returns a non-null intrusive_ptr") {
  actor_system sys{cfg};
  auto lg = detail::asynchronous_logger::make(sys);
  check_ne(lg, nullptr);
}

TEST("current_logger get and set") {
  SECTION("can be set to null") {
    logger::current_logger(nullptr);
    check_eq(logger::current_logger(), nullptr);
  }
  SECTION("is automatically set when creating an actor system") {
    actor_system sys{cfg};
    check_ne(logger::current_logger(), nullptr);
  }
  SECTION("can be manually set to a specific actor system") {
    actor_system sys{cfg};
    logger::current_logger(nullptr);
    check_eq(logger::current_logger(), nullptr);
    logger::current_logger(&sys);
    check_eq(logger::current_logger(), &sys.logger());
  }
  SECTION("is null after actor system is destroyed") {
    {
      actor_system sys{cfg};
    }
    check_eq(logger::current_logger(), nullptr);
  }
}

TEST("thread_local_aid returns the current actor ID") {
  SECTION("returns zero when no actor is active") {
    check_eq(logger::thread_local_aid(), 0u);
  }
  SECTION("returns the ID of a scoped actor while it receives a message") {
    actor_system sys{cfg};
    scoped_actor self{sys};
    anon_mail(7).send(self);
    auto received = std::make_shared<bool>(false);
    self->receive([this, received, aid = self->id()](int value) {
      check_eq(value, 7);
      check_eq(logger::thread_local_aid(), aid);
      *received = true;
    });
    check(*received);
  }
  SECTION("returns the ID of a scheduled actor while it is running") {
    actor_system sys{cfg};
    auto observed_id = std::make_shared<std::promise<actor_id>>();
    auto hdl = sys.spawn([observed_id](event_based_actor*) {
      observed_id->set_value(logger::thread_local_aid());
    });
    check_eq(observed_id->get_future().get(), hdl.id());
  }
}

SCENARIO("logger accepts returns false for levels above verbosity") {
  GIVEN("an actor system with warning verbosity") {
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

SCENARIO("loggers reject excluded components") {
  GIVEN("an actor system with excluded core component for file and console") {
    cfg.set("caf.logger.console.excluded-components", R"(["core"])");
    cfg.set("caf.logger.file.excluded-components", R"(["core"])");
    cfg.set("caf.logger.console.verbosity", "trace");
    cfg.set("caf.logger.file.verbosity", "trace");
    actor_system sys{cfg};
    WHEN("checking accepts for core component") {
      THEN("accepts is false for all levels") {
        check(!sys.logger().accepts(log::level::error, "core"));
        check(!sys.logger().accepts(log::level::warning, "core"));
        check(!sys.logger().accepts(log::level::info, "core"));
        check(!sys.logger().accepts(log::level::debug, "core"));
        check(!sys.logger().accepts(log::level::trace, "core"));
      }
    }
  }
  GIVEN("an actor system with excluded core component for console only") {
    cfg.set("caf.logger.console.excluded-components", R"(["core"])");
    cfg.set("caf.logger.console.verbosity", "trace");
    cfg.set("caf.logger.file.verbosity", "trace");
    actor_system sys{cfg};
    WHEN("checking accepts for core component") {
      THEN("accepts is true for all levels") {
        check(sys.logger().accepts(log::level::error, "core"));
        check(sys.logger().accepts(log::level::warning, "core"));
        check(sys.logger().accepts(log::level::info, "core"));
        check(sys.logger().accepts(log::level::debug, "core"));
        check(sys.logger().accepts(log::level::trace, "core"));
      }
    }
  }
  GIVEN("an actor system with excluded core component for file only") {
    cfg.set("caf.logger.file.excluded-components", R"(["core"])");
    cfg.set("caf.logger.console.verbosity", "trace");
    cfg.set("caf.logger.file.verbosity", "trace");
    actor_system sys{cfg};
    WHEN("checking accepts for core component") {
      THEN("accepts is true for all levels") {
        check(sys.logger().accepts(log::level::error, "core"));
        check(sys.logger().accepts(log::level::warning, "core"));
        check(sys.logger().accepts(log::level::info, "core"));
        check(sys.logger().accepts(log::level::debug, "core"));
        check(sys.logger().accepts(log::level::trace, "core"));
      }
    }
  }
}

} // WITH_FIXTURE(fixture)
