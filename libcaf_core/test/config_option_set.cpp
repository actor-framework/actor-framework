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

#include <map>
#include <string>
#include <vector>

#include "caf/config.hpp"

#define CAF_SUITE config_option_set
#include "caf/test/dsl.hpp"

#include "caf/config_option_set.hpp"

using std::string;
using std::vector;

using namespace caf;

namespace {

struct fixture {
  config_option_set opts;
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(config_option_set_tests, fixture)

CAF_TEST(lookup) {
  opts.add<int>("opt1,1", "test option 1")
    .add<float>("test", "opt2,2", "test option 2")
    .add<bool>("test", "flag,fl3", "test flag");
  CAF_CHECK_EQUAL(opts.size(), 3u);
  CAF_MESSAGE("lookup by long name");
  CAF_CHECK_NOT_EQUAL(opts.cli_long_name_lookup("opt1"), nullptr);
  CAF_CHECK_NOT_EQUAL(opts.cli_long_name_lookup("global.opt1"), nullptr);
  CAF_CHECK_NOT_EQUAL(opts.cli_long_name_lookup("test.opt2"), nullptr);
  CAF_CHECK_NOT_EQUAL(opts.cli_long_name_lookup("test.flag"), nullptr);
  CAF_MESSAGE("lookup by short name");
  CAF_CHECK_NOT_EQUAL(opts.cli_short_name_lookup('1'), nullptr);
  CAF_CHECK_NOT_EQUAL(opts.cli_short_name_lookup('2'), nullptr);
  CAF_CHECK_NOT_EQUAL(opts.cli_short_name_lookup('f'), nullptr);
  CAF_CHECK_NOT_EQUAL(opts.cli_short_name_lookup('l'), nullptr);
  CAF_CHECK_NOT_EQUAL(opts.cli_short_name_lookup('3'), nullptr);
}

CAF_TEST(parse with ref syncing) {
  using ls = vector<string>; // list of strings
  using ds = dictionary<string>; // dictionary of strings
  auto foo_i = 0;
  auto foo_f = 0.f;
  auto foo_b = false;
  auto bar_s = string{};
  auto bar_l = ls{};
  auto bar_d = ds{};
  opts.add<int>(foo_i, "foo", "i,i", "")
    .add<float>(foo_f, "foo", "f,f", "")
    .add<bool>(foo_b, "foo", "b,b", "")
    .add<string>(bar_s, "bar", "s,s", "")
    .add<vector<string>>(bar_l, "bar", "l,l", "")
    .add<dictionary<string>>(bar_d, "bar", "d,d", "");
  dictionary<config_value::dictionary> cfg;
  vector<string> args{"-i42",
                      "-f",
                      "1e12",
                      "-shello",
                      "--bar.l=[\"hello\", \"world\"]",
                      "-d",
                      "{a=\"a\",b=\"b\"}",
                      "-b"};
  CAF_MESSAGE("parse arguments");
  auto res = opts.parse(cfg, args);
  CAF_CHECK_EQUAL(res.first, pec::success);
  if (res.second != args.end())
    CAF_FAIL("parser stopped at: " << *res.second);
  CAF_MESSAGE("verify referenced values");
  CAF_CHECK_EQUAL(foo_i, 42);
  CAF_CHECK_EQUAL(foo_f, 1e12);
  CAF_CHECK_EQUAL(foo_b, true);
  CAF_CHECK_EQUAL(bar_s, "hello");
  CAF_CHECK_EQUAL(bar_l, ls({"hello", "world"}));
  CAF_CHECK_EQUAL(bar_d, ds({{"a", "a"}, {"b", "b"}}));
  CAF_MESSAGE("verify dictionary content");
  CAF_CHECK_EQUAL(get<int>(cfg, "foo.i"), 42);
}

CAF_TEST(implicit global) {
  opts.add<int>("value", "some value").add<bool>("help", "print help text");
  CAF_MESSAGE("test long option with argument");
  dictionary<config_value::dictionary> cfg;
  auto res = opts.parse(cfg, {"--value=42"});
  CAF_CHECK_EQUAL(res.first, pec::success);
  CAF_CHECK_EQUAL(get_if<int>(&cfg, "global.value"), 42);
  CAF_MESSAGE("test long option flag");
  cfg.clear();
  res = opts.parse(cfg, {"--help"});
  CAF_CHECK_EQUAL(res.first, pec::success);
  CAF_CHECK_EQUAL(get_or(cfg, "global.help", false), true);
}

CAF_TEST(atom parameters) {
  opts.add<atom_value>("value,v", "some value");
  CAF_MESSAGE("test atom option without quotes");
  auto parse_args = [&](std::vector<std::string> args) -> expected<atom_value> {
    dictionary<config_value::dictionary> cfg;
    auto res = opts.parse(cfg, std::move(args));
    if (res.first != pec::success)
      return res.first;
    auto atm = get_if<atom_value>(&cfg, "global.value");
    if (atm == none)
      return sec::invalid_argument;
    return *atm;
  };
  CAF_CHECK_EQUAL(parse_args({"-v", "'foobar'"}), atom("foobar"));
  CAF_CHECK_EQUAL(parse_args({"-v'foobar'"}), atom("foobar"));
  CAF_CHECK_EQUAL(parse_args({"--value='foobar'"}), atom("foobar"));
  CAF_CHECK_EQUAL(parse_args({"-v", "foobar"}), atom("foobar"));
  CAF_CHECK_EQUAL(parse_args({"-vfoobar"}), atom("foobar"));
  CAF_CHECK_EQUAL(parse_args({"--value=foobar"}), atom("foobar"));
}

CAF_TEST_FIXTURE_SCOPE_END()
