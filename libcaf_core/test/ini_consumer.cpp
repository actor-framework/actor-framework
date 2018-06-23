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
[global]
is_server=true
port=4242
nodes=["sun", "venus", ]
[logger]
file-name = "foobar.ini" ; our file name
[scheduler] ; more settings
  timing  =  2us ; using microsecond resolution
impl =       'foo';some atom
)";

struct fixture {
  detail::parser::state<std::string::const_iterator> res;
  config_option_set options;
  config_option_set::config_map config;

  fixture() {
    options.add<bool>("global", "is_server")
      .add<uint16_t>("global", "port")
      .add<ls>("global", "nodes")
      .add<string>("logger", "file-name")
      .add<int>("scheduler", "padding")
      .add<timespan>("scheduler", "timing")
      .add<atom_value>("scheduler", "impl");
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
  CAF_CHECK_EQUAL(get<bool>(config, "global.is_server"), true);
  CAF_CHECK_EQUAL(get<uint16_t>(config, "global.port"), 4242u);
  CAF_CHECK_EQUAL(get<ls>(config, "global.nodes"), ls({"sun", "venus"}));
  CAF_CHECK_EQUAL(get<string>(config, "logger.file-name"), "foobar.ini");
  CAF_CHECK_EQUAL(get<timespan>(config, "scheduler.timing"), timespan(2000));
  CAF_CHECK_EQUAL(get<atom_value>(config, "scheduler.impl"), atom("foo"));
}

CAF_TEST_FIXTURE_SCOPE_END()
