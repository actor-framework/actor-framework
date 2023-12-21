// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/config_consumer.hpp"

#include "caf/test/test.hpp"

#include "caf/detail/parser/read_config.hpp"
#include "caf/log/test.hpp"

using std::string;

using namespace caf;

// List-of-strings.
using ls = std::vector<std::string>;

namespace {

constexpr const std::string_view test_config1 = R"(
is_server=true
port=4242
nodes=["sun", "venus", ]
logger{
  file-name = "foobar.conf" # our file name
}
scheduler { # more settings
  timing  =  2us # using microsecond resolution
}
)";

constexpr const std::string_view test_config2 = R"(
is_server = true
logger = {
  file-name = "foobar.conf"
}
port = 4242
scheduler : {
  timing = 2us,
}
nodes = ["sun", "venus"]
)";

struct fixture {
  config_option_set options;
  settings config;

  fixture() {
    options.add<bool>("global", "is_server", "enables server mode")
      .add<uint16_t>("global", "port", "sets local or remote port")
      .add<ls>("global", "nodes", "list of remote nodes")
      .add<string>("logger", "file-name", "log output file")
      .add<int>("scheduler", "padding", "some integer")
      .add<timespan>("scheduler", "timing", "some timespan");
  }
};

WITH_FIXTURE(fixture) {

TEST("config_consumer") {
  std::string_view str = test_config1;
  detail::config_consumer consumer{options, config};
  string_parser_state res{str.begin(), str.end()};
  detail::parser::read_config(res, consumer);
  check(res.i == res.e);
  check_eq(res.code, pec::success);
  check_eq(get_as<bool>(config, "is_server"), true);
  check_eq(get_as<uint16_t>(config, "port"), 4242u);
  check_eq(get_as<ls>(config, "nodes"), ls({"sun", "venus"}));
  check_eq(get_as<string>(config, "logger.file-name"), "foobar.conf");
  check_eq(get_as<timespan>(config, "scheduler.timing"), timespan(2000));
}

TEST("simplified syntax") {
  log::test::debug("read test_config");
  {
    detail::config_consumer consumer{options, config};
    string_parser_state res{test_config1.begin(), test_config1.end()};
    detail::parser::read_config(res, consumer);
    check_eq(res.code, pec::success);
  }
  settings config2;
  log::test::debug("read test_config2");
  {
    detail::config_consumer consumer{options, config2};
    string_parser_state res{test_config2.begin(), test_config2.end()};
    detail::parser::read_config(res, consumer);
    check_eq(res.code, pec::success);
  }
  check_eq(config, config2);
}

} // WITH_FIXTURE(fixture)

} // namespace
