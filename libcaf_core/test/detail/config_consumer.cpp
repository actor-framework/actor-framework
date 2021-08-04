// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE detail.config_consumer

#include "caf/detail/config_consumer.hpp"

#include "caf/test/dsl.hpp"

#include "caf/detail/parser/read_config.hpp"

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

} // namespace

CAF_TEST_FIXTURE_SCOPE(config_consumer_tests, fixture)

CAF_TEST(config_consumer) {
  std::string_view str = test_config1;
  detail::config_consumer consumer{options, config};
  string_parser_state res{str.begin(), str.end()};
  detail::parser::read_config(res, consumer);
  CAF_CHECK_EQUAL(res.code, pec::success);
  CAF_CHECK_EQUAL(std::string_view(std::addressof(*res.i),
                                   static_cast<size_t>(res.e - res.i)),
                  std::string_view());
  CAF_CHECK_EQUAL(get_as<bool>(config, "is_server"), true);
  CAF_CHECK_EQUAL(get_as<uint16_t>(config, "port"), 4242u);
  CAF_CHECK_EQUAL(get_as<ls>(config, "nodes"), ls({"sun", "venus"}));
  CAF_CHECK_EQUAL(get_as<string>(config, "logger.file-name"), "foobar.conf");
  CAF_CHECK_EQUAL(get_as<timespan>(config, "scheduler.timing"), timespan(2000));
}

CAF_TEST(simplified syntax) {
  CAF_MESSAGE("read test_config");
  {
    detail::config_consumer consumer{options, config};
    string_parser_state res{test_config1.begin(), test_config1.end()};
    detail::parser::read_config(res, consumer);
    CAF_CHECK_EQUAL(res.code, pec::success);
  }
  settings config2;
  CAF_MESSAGE("read test_config2");
  {
    detail::config_consumer consumer{options, config2};
    string_parser_state res{test_config2.begin(), test_config2.end()};
    detail::parser::read_config(res, consumer);
    CAF_CHECK_EQUAL(res.code, pec::success);
  }
  CAF_CHECK_EQUAL(config, config2);
}

CAF_TEST_FIXTURE_SCOPE_END()
