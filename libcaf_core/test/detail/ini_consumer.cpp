/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#define CAF_SUITE detail.ini_consumer

#include "caf/detail/ini_consumer.hpp"

#include "caf/test/dsl.hpp"

#include "caf/detail/parser/read_ini.hpp"

using std::string;

using namespace caf;

// List-of-strings.
using ls = std::vector<std::string>;

namespace {

constexpr const string_view test_ini = R"(
is_server=true
port=4242
nodes=["sun", "venus", ]
[logger]
file-name = "foobar.ini" ; our file name
[scheduler] ; more settings
  timing  =  2us ; using microsecond resolution
)";

constexpr const string_view test_ini2 = R"(
is_server = true
logger = {
  file-name = "foobar.ini"
}
port = 4242
scheduler = {
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

CAF_TEST_FIXTURE_SCOPE(ini_consumer_tests, fixture)

CAF_TEST(ini_value_consumer) {
  string_view str = R"("hello world")";
  detail::ini_value_consumer consumer;
  string_parser_state res{str.begin(), str.end()};
  detail::parser::read_ini_value(res, consumer);
  CAF_CHECK_EQUAL(res.code, pec::success);
  CAF_CHECK_EQUAL(get<string>(consumer.result), "hello world");
}

CAF_TEST(ini_consumer) {
  string_view str = test_ini;
  detail::ini_consumer consumer{options, config};
  string_parser_state res{str.begin(), str.end()};
  detail::parser::read_ini(res, consumer);
  CAF_CHECK_EQUAL(res.code, pec::success);
  CAF_CHECK_EQUAL(get<bool>(config, "is_server"), true);
  CAF_CHECK_EQUAL(get<uint16_t>(config, "port"), 4242u);
  CAF_CHECK_EQUAL(get<ls>(config, "nodes"), ls({"sun", "venus"}));
  CAF_CHECK_EQUAL(get<string>(config, "logger.file-name"), "foobar.ini");
  CAF_MESSAGE(config);
  CAF_CHECK_EQUAL(get<timespan>(config, "scheduler.timing"), timespan(2000));
}

CAF_TEST(simplified syntax) {
  CAF_MESSAGE("read test_ini");
  {
    detail::ini_consumer consumer{options, config};
    string_parser_state res{test_ini.begin(), test_ini.end()};
    detail::parser::read_ini(res, consumer);
    CAF_CHECK_EQUAL(res.code, pec::success);
  }
  settings config2;
  CAF_MESSAGE("read test_ini2");
  {
    detail::ini_consumer consumer{options, config2};
    string_parser_state res{test_ini2.begin(), test_ini2.end()};
    detail::parser::read_ini(res, consumer);
    CAF_CHECK_EQUAL(res.code, pec::success);
  }
  CAF_CHECK_EQUAL(config, config2);
}

CAF_TEST_FIXTURE_SCOPE_END()
