// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/async/file.hpp"

#include "caf/test/caf_test_main.hpp"
#include "caf/test/fixture/flow.hpp"
#include "caf/test/nil.hpp"
#include "caf/test/test.hpp"

#include "caf/event_based_actor.hpp"
#include "caf/flow/observable.hpp"

#include <chrono>
#include <future>
#include <string>

using namespace caf;
using namespace std::literals;

constexpr const char* quotes_file = CAF_TEST_DATA_DIR "/quotes.txt";

constexpr const char* invalid_file = CAF_TEST_DATA_DIR "/does-not-exist.txt";

constexpr std::string_view quotes_uppercase = R"_(
TRUE HAPPINESS IS FOUND WITHIN,
FOR EXTERNAL CIRCUMSTANCES ARE BEYOND OUR CONTROL.)_";

constexpr std::string_view quotes_numbered_lines = R"_(1:
2:True happiness is found within,
3:for external circumstances are beyond our control.
4:
5:The key to peace of mind lies in accepting what we cannot change
6:and focusing on what we can.
7:
)_";

TEST("async file I/O") {
  actor_system_config cfg;
  actor_system sys{cfg};
  auto prom = std::make_shared<std::promise<expected<std::string>>>();
  auto res = prom->get_future();
  SECTION("read chars from file") {
    auto pub = async::file(sys, quotes_file).read_chars().run([](auto&& src) {
      return std::move(src)
        .map([](char ch) { return static_cast<char>(toupper(ch)); })
        .take(83)
        .to_publisher();
    });
    sys.spawn([pub, prom](event_based_actor* self) mutable {
      auto str = std::make_shared<std::string>();
      pub.observe_on(self)
        .do_on_error([prom](const error& err) { prom->set_value(err); })
        .do_on_complete([prom, str] { prom->set_value(std::move(*str)); })
        .for_each([str](char ch) mutable { *str += ch; });
    });
    if (res.wait_for(2s) != std::future_status::ready) {
      fail("timeout");
    }
    check_eq(res.get(), quotes_uppercase);
  }
  SECTION("read lines from file") {
    auto pub = async::file(sys, quotes_file).read_lines().run();
    sys.spawn([pub, prom](event_based_actor* self) mutable {
      auto str = std::make_shared<std::string>();
      pub.observe_on(self)
        .do_on_error([prom](const error& err) { prom->set_value(err); })
        .do_on_complete([prom, str] { prom->set_value(std::move(*str)); })
        .for_each([str, line = 1](const cow_string& cs) mutable {
          *str += std::to_string(line++);
          *str += ':';
          *str += cs.str();
          *str += '\n';
        });
    });
    if (res.wait_for(2s) != std::future_status::ready) {
      fail("timeout");
    }
    check_eq(res.get(), quotes_numbered_lines);
  }
  SECTION("try to read from non-existing file") {
    auto invalid = async::file(sys, invalid_file);
    SECTION("read_chars") {
      auto pub = invalid.read_chars().run();
      sys.spawn([pub, prom](event_based_actor* self) mutable {
        auto str = std::make_shared<std::string>();
        pub.observe_on(self)
          .do_on_error([prom](const error& err) { prom->set_value(err); })
          .do_on_complete([prom, str] { prom->set_value(std::move(*str)); })
          .for_each([str](char ch) mutable { *str += ch; });
      });
      if (res.wait_for(2s) != std::future_status::ready) {
        fail("timeout");
      }
      check_eq(res.get(), error{sec::cannot_open_file});
    }
    SECTION("read_lines") {
      auto pub = invalid.read_lines().run();
      sys.spawn([pub, prom](event_based_actor* self) mutable {
        auto str = std::make_shared<std::string>();
        pub.observe_on(self)
          .do_on_error([prom](const error& err) { prom->set_value(err); })
          .do_on_complete([prom, str] { prom->set_value(std::move(*str)); })
          .for_each([str](const cow_string& cs) mutable { *str += cs.str(); });
      });
      if (res.wait_for(2s) != std::future_status::ready) {
        fail("timeout");
      }
      check_eq(res.get(), error{sec::cannot_open_file});
    }
  }
}

CAF_TEST_MAIN()
