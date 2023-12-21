// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/log_level_map.hpp"

#include "caf/test/test.hpp"

#include "caf/log/level.hpp"

#include <map>

using namespace caf;
using namespace std::literals;

using caf::log::level;

namespace {

TEST("log level maps render the default log levels") {
  detail::log_level_map uut;
  check_eq(uut[level::quiet], "OFF");
  check_eq(uut[level::quiet + 1], "OFF");
  check_eq(uut[level::error - 1], "OFF");
  check_eq(uut[level::error], "ERROR");
  check_eq(uut[level::error + 1], "ERROR");
  check_eq(uut[level::warning - 1], "ERROR");
  check_eq(uut[level::warning], "WARNING");
  check_eq(uut[level::warning + 1], "WARNING");
  check_eq(uut[level::info - 1], "WARNING");
  check_eq(uut[level::info], "INFO");
  check_eq(uut[level::info + 1], "INFO");
  check_eq(uut[level::debug - 1], "INFO");
  check_eq(uut[level::debug], "DEBUG");
  check_eq(uut[level::debug + 1], "DEBUG");
  check_eq(uut[level::trace - 1], "DEBUG");
  check_eq(uut[level::trace], "TRACE");
  check_eq(uut[level::trace + 1], "TRACE");
}

TEST("log level maps allows custom log levels") {
  std::map<std::string, unsigned> custom{{"NOTICE"s, CAF_LOG_LEVEL_WARNING + 1},
                                         {"VERBOSE"s, CAF_LOG_LEVEL_INFO + 1}};
  detail::log_level_map uut;
  uut.set(custom);
  check_eq(uut[level::quiet], "OFF");
  check_eq(uut[level::quiet + 1], "OFF");
  check_eq(uut[level::error - 1], "OFF");
  check_eq(uut[level::error], "ERROR");
  check_eq(uut[level::error + 1], "ERROR");
  check_eq(uut[level::warning - 1], "ERROR");
  check_eq(uut[level::warning], "WARNING");
  check_eq(uut[level::warning + 1], "NOTICE");
  check_eq(uut[level::info - 1], "NOTICE");
  check_eq(uut[level::info], "INFO");
  check_eq(uut[level::info + 1], "VERBOSE");
  check_eq(uut[level::debug - 1], "VERBOSE");
  check_eq(uut[level::debug], "DEBUG");
  check_eq(uut[level::debug + 1], "DEBUG");
  check_eq(uut[level::trace - 1], "DEBUG");
  check_eq(uut[level::trace], "TRACE");
  check_eq(uut[level::trace + 1], "TRACE");
}

TEST("log level maps allow overriding default log level names") {
  std::map<std::string, unsigned> custom{{"my-quiet"s, CAF_LOG_LEVEL_QUIET},
                                         {"my-info"s, CAF_LOG_LEVEL_INFO}};
  detail::log_level_map uut;
  uut.set(custom);
  check_eq(uut[level::quiet], "my-quiet");
  check_eq(uut[level::quiet + 1], "my-quiet");
  check_eq(uut[level::error - 1], "my-quiet");
  check_eq(uut[level::error], "ERROR");
  check_eq(uut[level::error + 1], "ERROR");
  check_eq(uut[level::warning - 1], "ERROR");
  check_eq(uut[level::warning], "WARNING");
  check_eq(uut[level::warning + 1], "WARNING");
  check_eq(uut[level::info - 1], "WARNING");
  check_eq(uut[level::info], "my-info");
  check_eq(uut[level::info + 1], "my-info");
  check_eq(uut[level::debug - 1], "my-info");
  check_eq(uut[level::debug], "DEBUG");
  check_eq(uut[level::debug + 1], "DEBUG");
  check_eq(uut[level::trace - 1], "DEBUG");
  check_eq(uut[level::trace], "TRACE");
  check_eq(uut[level::trace + 1], "TRACE");
}

TEST("log level maps allow case-insensitive lookup by name") {
  std::map<std::string, unsigned> custom{{"NOTICE"s, CAF_LOG_LEVEL_WARNING + 1},
                                         {"VERBOSE"s, CAF_LOG_LEVEL_INFO + 1}};
  detail::log_level_map uut;
  uut.set(custom);
  check_eq(uut.by_name("foo"), level::quiet); // default
  check_eq(uut.by_name("OFF"), level::quiet);
  check_eq(uut.by_name("off"), level::quiet);
  check_eq(uut.by_name("ERROR"), level::error);
  check_eq(uut.by_name("error"), level::error);
  check_eq(uut.by_name("WARNING"), level::warning);
  check_eq(uut.by_name("warning"), level::warning);
  check_eq(uut.by_name("NOTICE"), level::warning + 1);
  check_eq(uut.by_name("notice"), level::warning + 1);
  check_eq(uut.by_name("INFO"), level::info);
  check_eq(uut.by_name("info"), level::info);
  check_eq(uut.by_name("VERBOSE"), level::info + 1);
  check_eq(uut.by_name("verbose"), level::info + 1);
  check_eq(uut.by_name("DEBUG"), level::debug);
  check_eq(uut.by_name("debug"), level::debug);
  check_eq(uut.by_name("TRACE"), level::trace);
  check_eq(uut.by_name("trace"), level::trace);
}

} // namespace
