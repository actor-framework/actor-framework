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

#include "caf/config.hpp"

#define CAF_SUITE ini_consumer
#include "caf/test/dsl.hpp"

#include "caf/detail/ini_consumer.hpp"
#include "caf/detail/parser/read_ini.hpp"

using std::string;

using namespace caf;

// List-of-strings.
using ls = std::vector<std::string>;

namespace {

const char test_ini[] = R"(
is_server=true
port=4242
nodes=["sun", "venus", ]
[logger]
file-name = "foobar.ini" ; our file name
[scheduler] ; more settings
  timing  =  2us ; using microsecond resolution
impl =       'foo';some atom
)";

const char test_ini2[] = R"(
is_server = true
logger = {
  file-name = "foobar.ini"
}
port = 4242
scheduler = {
  timing = 2us,
  impl = 'foo'
}
nodes = ["sun", "venus"]
)";

struct fixture {
  detail::parser::state<std::string::const_iterator> res;
  config_option_set options;
  settings config;

  fixture() {
    options.add<bool>("global", "is_server", "enables server mode")
      .add<uint16_t>("global", "port", "sets local or remote port")
      .add<ls>("global", "nodes", "list of remote nodes")
      .add<string>("logger", "file-name", "log output file")
      .add<int>("scheduler", "padding", "some integer")
      .add<timespan>("scheduler", "timing", "some timespan")
      .add<atom_value>("scheduler", "impl", "some atom");
  }
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(ini_consumer_tests, fixture)

CAF_TEST(ini_value_consumer) {
  std::string str = R"("hello world")";
  detail::ini_value_consumer consumer;
  res.i = str.begin();
  res.e = str.end();
  detail::parser::read_ini_value(res, consumer);
  CAF_CHECK_EQUAL(res.code, pec::success);
  CAF_CHECK_EQUAL(get<string>(consumer.result), "hello world");
}

CAF_TEST(ini_consumer) {
  std::string str = test_ini;
  detail::ini_consumer consumer{options, config};
  res.i = str.begin();
  res.e = str.end();
  detail::parser::read_ini(res, consumer);
  CAF_CHECK_EQUAL(res.code, pec::success);
  CAF_CHECK_EQUAL(get<bool>(config, "is_server"), true);
  CAF_CHECK_EQUAL(get<uint16_t>(config, "port"), 4242u);
  CAF_CHECK_EQUAL(get<ls>(config, "nodes"), ls({"sun", "venus"}));
  CAF_CHECK_EQUAL(get<string>(config, "logger.file-name"), "foobar.ini");
  CAF_CHECK_EQUAL(get<timespan>(config, "scheduler.timing"), timespan(2000));
  CAF_CHECK_EQUAL(get<atom_value>(config, "scheduler.impl"), atom("foo"));
}

CAF_TEST(simplified syntax) {
  std::string str = test_ini;
  CAF_MESSAGE("read test_ini");
  {
    detail::ini_consumer consumer{options, config};
    res.i = str.begin();
    res.e = str.end();
    detail::parser::read_ini(res, consumer);
    CAF_CHECK_EQUAL(res.code, pec::success);
  }
  str = test_ini2;
  settings config2;
  CAF_MESSAGE("read test_ini2");
  {
    detail::ini_consumer consumer{options, config2};
    res.i = str.begin();
    res.e = str.end();
    detail::parser::read_ini(res, consumer);
    CAF_CHECK_EQUAL(res.code, pec::success);
  }
  CAF_CHECK_EQUAL(config, config2);
}

CAF_TEST_FIXTURE_SCOPE_END()
