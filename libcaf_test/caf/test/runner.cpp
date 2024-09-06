// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/test/runner.hpp"

#include "caf/test/context.hpp"
#include "caf/test/nesting_error.hpp"
#include "caf/test/reporter.hpp"
#include "caf/test/runnable.hpp"

#include "caf/config_option.hpp"
#include "caf/config_option_adder.hpp"
#include "caf/config_option_set.hpp"
#include "caf/detail/log_level.hpp"
#include "caf/detail/set_thread_name.hpp"
#include "caf/log/level.hpp"

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <optional>
#include <regex>
#include <string>
#include <thread>

namespace caf::test {

namespace {

class watchdog {
public:
  explicit watchdog(int secs) {
    if (secs > 0)
      run(secs);
  }
  ~watchdog() {
    if (!thread_.joinable())
      return;
    {
      std::lock_guard<std::mutex> guard{mtx_};
      canceled_ = true;
      cv_.notify_all();
    }
    thread_.join();
  }

private:
  void run(int secs) {
    thread_ = std::thread{[this, secs] {
      caf::detail::set_thread_name("test.watchdog");
      auto tp = std::chrono::high_resolution_clock::now()
                + std::chrono::seconds(secs);
      std::unique_lock<std::mutex> guard{mtx_};
      while (!canceled_
             && cv_.wait_until(guard, tp) != std::cv_status::timeout) {
      }
      if (!canceled_) {
        auto err_out = [] { return std::ostream_iterator<char>{std::cerr}; };
        detail::format_to(err_out(),
                          "WATCHDOG: unit test exceeded {} seconds\n", secs);
        abort();
      }
    }};
  }

  bool canceled_ = false;
  std::mutex mtx_;
  std::condition_variable cv_;
  std::thread thread_;
};

config_option_set make_option_set() {
  config_option_set result;
  config_option_adder{result, "global"}
    .add<bool>("available-suites,a", "print all available suites")
    .add<bool>("help,h?", "print this help text")
    .add<bool>("no-colors,n", "disable coloring (ignored on Windows)")
    .add<size_t>("max-runtime,r", "set a maximum runtime in seconds")
    .add<std::string>("suites,s", "regex for selecting suites")
    .add<std::string>("tests,t", "regex for selecting tests")
    .add<std::string>("available-tests,A", "print tests for a suite")
    .add<std::string>("verbosity,v", "set verbosity level of the reporter")
    .add<std::vector<std::string>>("log-component-filter,l", "set log filter");
  return result;
}

std::optional<unsigned> parse_log_level(std::string_view x) {
  // Note: the 0-5 aliases are for compatibility with the old unit testing
  //       framework.
  if (x == "quiet" || x == "0")
    return log::level::quiet;
  if (x == "error" || x == "1")
    return log::level::error;
  if (x == "warning" || x == "2")
    return log::level::warning;
  if (x == "info" || x == "3")
    return log::level::info;
  if (x == "debug" || x == "4")
    return log::level::debug;
  if (x == "trace" || x == "5")
    return log::level::trace;
  return {};
}

std::optional<std::regex> to_regex(std::string_view regex_string) {
#ifdef CAF_ENABLE_EXCEPTIONS
  auto err_out = [] { return std::ostream_iterator<char>{std::cerr}; };
  try {
    return std::regex{regex_string.begin(), regex_string.end()};
  } catch (const std::exception& err) {
    detail::format_to(err_out(), "error while parsing argument '{}': {}\n",
                      regex_string, err.what());
    return std::nullopt;
  } catch (...) {
    detail::format_to(err_out(),
                      "error while parsing argument '{}': unknown error\n",
                      regex_string);
    return std::nullopt;
  }
#else
  return std::regex{regex_string.begin(), regex_string.end()};
#endif
}

auto default_log_component_filter() {
  using namespace std::literals;
  return std::vector{
    "caf"s,        // legacy CAF logging
    "caf_flow"s,   // legacy CAF logging
    "caf.core"s,   // events from log::core
    "caf.io"s,     // events from log::io
    "caf.net"s,    // events from log::net
    "caf.openssl"s // events from log::openssl
  };
}

} // namespace

runner::runner() : suites_(caf::test::registry::suites()) {
  // nop
}

int runner::run(int argc, char** argv) {
  auto default_reporter = reporter::make_default();
  reporter::instance(default_reporter.get());
  auto default_logger = reporter::make_logger();
  if (auto [ok, help_printed] = parse_cli(argc, argv); !ok) {
    return EXIT_FAILURE;
  } else if (help_printed) {
    return EXIT_SUCCESS;
  }
  auto suite_regex = to_regex(get_or(cfg_, "suites", ".*"));
  auto test_regex = to_regex(get_or(cfg_, "tests", ".*"));
  if (!suite_regex || !test_regex) {
    return EXIT_FAILURE;
  }
  default_reporter->no_colors(get_or(cfg_, "no-colors", false));
  default_reporter->log_component_filter(
    get_or(cfg_, "log-component-filter", default_log_component_filter()));
  default_reporter->start();
  auto enabled = [](const std::regex& selected,
                    std::string_view search_string) {
    return std::regex_search(search_string.begin(), search_string.end(),
                             selected);
  };
  watchdog runtime_guard{get_or(cfg_, "max-runtime", 0)};
  for (auto& [suite_name, suite] : suites_) {
    if (!enabled(*suite_regex, suite_name))
      continue;
    default_reporter->begin_suite(suite_name);
    for (auto [test_name, factory_instance] : suite) {
      if (!enabled(*test_regex, test_name))
        continue;
      auto state = std::make_shared<context>();
#ifdef CAF_ENABLE_EXCEPTIONS
      // Must be outside of the try block to make sure the object still exists
      // in the catch block.
      std::unique_ptr<runnable> def;
      try {
        do {
          logger::current_logger(default_logger.get());
          default_reporter->begin_test(state, test_name);
          def = factory_instance->make(state);
          def->run();
          default_reporter->end_test();
          state->clear_stacks();
        } while (state->can_run());
      } catch (const nesting_error& ex) {
        default_reporter->unhandled_exception(ex.message(), ex.location());
        default_reporter->end_test();
      } catch (const requirement_failed& ex) {
        auto event = log::event::make(log::level::error, "caf.test",
                                      ex.location(), 0,
                                      "requirement failed: {}", ex.message());
        default_reporter->print(event);
        default_reporter->end_test();
      } catch (const std::exception& ex) {
        default_reporter->unhandled_exception(ex.what());
        default_reporter->end_test();
      } catch (...) {
        default_reporter->unhandled_exception("unknown exception type");
        default_reporter->end_test();
      }
#else
      do {
        default_reporter->begin_test(state, test_name);
        auto def = factory_instance->make(state);
        def->run();
        default_reporter->end_test();
        state->clear_stacks();
      } while (state->can_run());
#endif
    }
    default_reporter->end_suite(suite_name);
  }
  default_reporter->stop();
  return default_reporter->success() ? EXIT_SUCCESS : EXIT_FAILURE;
}

runner::parse_cli_result runner::parse_cli(int argc, char** argv) {
  std::vector<std::string> args_cpy{argv + 1, argv + argc};
  auto options = make_option_set();
  auto res = options.parse(cfg_, args_cpy);
  auto err = std::ostream_iterator<char>{std::cerr};
  if (res.first != caf::pec::success) {
    detail::format_to(err, "error while parsing argument '{}': {}\n\n{}\n",
                      *res.second, to_string(res.first), options.help_text());
    return {false, true};
  }
  if (get_or(cfg_, "help", false)) {
    detail::format_to(err, "{}\n", options.help_text());
    return {true, true};
  }
  if (get_or(cfg_, "available-suites", false)) {
    detail::format_to(err, "available suites:\n");
    for (auto& [suite_name, suite] : suites_)
      detail::format_to(err, "- {}\n", suite_name);
    return {true, true};
  }
  if (auto suite_name = get_as<std::string>(cfg_, "available-tests")) {
    auto i = suites_.find(*suite_name);
    if (i == suites_.end()) {
      detail::format_to(err, "no such suite: {}\n", *suite_name);
      return {false, true};
    }
    detail::format_to(err, "available tests in suite {}:\n", i->first);
    for (const auto& [test_name, factory_instance] : i->second)
      detail::format_to(err, "- {}\n", test_name);
    return {true, true};
  }
  if (auto verbosity = get_as<std::string>(cfg_, "verbosity")) {
    auto level = parse_log_level(*verbosity);
    if (!level) {
      detail::format_to(err,
                        "unrecognized verbosity level: '{}'\n"
                        "expected one of:\n"
                        "- quiet\n"
                        "- error\n"
                        "- warning\n"
                        "- info\n"
                        "- debug\n"
                        "- trace\n",
                        *verbosity);
      return {false, true};
    }
    reporter::instance().verbosity(*level);
  }
  return {true, false};
}

} // namespace caf::test
