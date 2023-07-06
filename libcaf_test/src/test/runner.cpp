// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/runner.hpp"

#include "caf/config_option.hpp"
#include "caf/config_option_adder.hpp"
#include "caf/config_option_set.hpp"
#include "caf/detail/log_level.hpp"
#include "caf/settings.hpp"
#include "caf/test/context.hpp"
#include "caf/test/nesting_error.hpp"
#include "caf/test/reporter.hpp"
#include "caf/test/runnable.hpp"

#include <iostream>
#include <optional>
#include <string>

namespace caf::test {

namespace {

  config_option_set make_option_set() {
    config_option_set result;
    config_option_adder{result, "global"}
      .add<bool>("help,h?", "print this help text")
      .add<bool>("available-suites,a", "print all available suites")
      .add<std::string>("available-tests,A", "print tests for a suite")
      .add<std::string>("verbosity,v", "set verbosity level of the reporter");
    return result;
  }

  std::optional<unsigned> parse_log_level(std::string_view x) {
    if (x == "quiet")
      return CAF_LOG_LEVEL_QUIET;
    if (x == "error")
      return CAF_LOG_LEVEL_ERROR;
    if (x == "warning")
      return CAF_LOG_LEVEL_WARNING;
    if (x == "info")
      return CAF_LOG_LEVEL_INFO;
    if (x == "debug")
      return CAF_LOG_LEVEL_DEBUG;
    if (x == "trace")
      return CAF_LOG_LEVEL_TRACE;
    return {};
  }

}//

runner::runner() : suites_(caf::test::registry::suites()) {
    // nop
}

int runner::run(int argc, char** argv) {
    auto default_reporter = reporter::make_default();
    reporter::instance = default_reporter.get();
    if (auto [ok, help_printed] = parse_cli(argc, argv); !ok) {
      return EXIT_FAILURE;
    } else if (help_printed) {
      return EXIT_SUCCESS;
    }
    default_reporter->start();
    for (auto& [suite_name, suite] : suites_) {
      default_reporter->begin_suite(suite_name);
      for (auto [test_name, factory_instance] : suite) {
        auto state = std::make_shared<context>();
        try {
          do {
            default_reporter->begin_test(state, test_name);
            auto def = factory_instance->make(state);
            def->run();
            default_reporter->end_test();
            state->clear_stacks();
          } while (state->can_run());
        } catch (const nesting_error& ex) {
          default_reporter->unhandled_exception(ex.message(), ex.location());
          default_reporter->end_test();
        } catch (const std::exception& ex) {
          default_reporter->unhandled_exception(ex.what());
          default_reporter->end_test();
        } catch (...) {
          default_reporter->unhandled_exception("unknown exception type");
          default_reporter->end_test();
        }
      }
      default_reporter->end_suite(suite_name);
    }
    default_reporter->stop();
    return default_reporter->success() > 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

runner::parse_cli_result runner::parse_cli(int argc, char** argv) {
    using detail::format_to;
    caf::settings cfg;
    std::vector<std::string> args_cpy{argv + 1, argv + argc};
    auto options = make_option_set();
    auto res = options.parse(cfg, args_cpy);
    auto err = std::ostream_iterator<char>{std::cerr};
    if (res.first != caf::pec::success) {
      format_to(err, "error while parsing argument '{}': {}\n\n{}\n",
                *res.second, to_string(res.first), options.help_text());
      return {false, true};
    }
    if (get_or(cfg, "help", false)) {
      format_to(err, "{}\n", options.help_text());
      return {true, true};
    }
    if (get_or(cfg, "available-suites", false)) {
      format_to(err, "available suites:\n");
      for (auto& [suite_name, suite] : suites_)
        format_to(err, "- {}\n", suite_name);
      return {true, true};
    }
    if (auto suite_name = get_as<std::string>(cfg, "available-tests")) {
      auto i = suites_.find(*suite_name);
      if (i == suites_.end()) {
        format_to(err, "no such suite: {}\n", *suite_name);
        return {false, true};
      }
      format_to(err, "available tests in suite {}:\n", i->first);
      for (const auto& [test_name, factory_instance] : i->second)
        format_to(err, "- {}\n", test_name);
      return {true, true};
    }
    if (auto verbosity = get_as<std::string>(cfg, "verbosity")) {
      auto level = parse_log_level(*verbosity);
      if (!level) {
        format_to(err,
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
      reporter::instance->verbosity(*level);
    }
    return {true, false};
}

} // namespace caf::test
