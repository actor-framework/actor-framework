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
#include "caf/settings.hpp"

using std::string;
using std::vector;

using namespace caf;

namespace {

struct fixture {
  config_option_set opts;

  template <class T>
  error read(settings& cfg, std::vector<std::string> args) {
    auto res = opts.parse(cfg, std::move(args));
    if (res.first != pec::success)
      return res.first;
    return none;
  }

  template <class T>
  expected<T> read(std::vector<std::string> args) {
    settings cfg;
    auto res = opts.parse(cfg, std::move(args));
    if (res.first != pec::success)
      return res.first;
    auto x = get_if<T>(&cfg, key);
    if (x == none)
      return sec::invalid_argument;
    return *x;
  }

  std::string key = "value";
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(config_option_set_tests, fixture)

CAF_TEST(lookup) {
  opts.add<int>("opt1,1", "test option 1")
    .add<float>("test", "opt2,2", "test option 2")
    .add<bool>("test", "flag,fl3", "test flag");
  CAF_CHECK_EQUAL(opts.size(), 3u);
  CAF_MESSAGE("lookup by long name");
  CAF_CHECK_NOT_EQUAL(opts.cli_long_name_lookup("opt1"), nullptr);
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
  settings cfg;
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

CAF_TEST(atom parameters) {
  opts.add<atom_value>("value,v", "some value");
  CAF_CHECK_EQUAL(read<atom_value>({"-v", "foobar"}), atom("foobar"));
  CAF_CHECK_EQUAL(read<atom_value>({"-vfoobar"}), atom("foobar"));
  CAF_CHECK_EQUAL(read<atom_value>({"--value=foobar"}), atom("foobar"));
}

CAF_TEST(string parameters) {
  opts.add<std::string>("value,v", "some value");
  CAF_MESSAGE("test string option with and without quotes");
  CAF_CHECK_EQUAL(read<std::string>({"--value=\"foo\\tbar\""}), "foo\tbar");
  CAF_CHECK_EQUAL(read<std::string>({"--value=foobar"}), "foobar");
  CAF_CHECK_EQUAL(read<std::string>({"-v", "\"foobar\""}), "foobar");
  CAF_CHECK_EQUAL(read<std::string>({"-v", "foobar"}), "foobar");
  CAF_CHECK_EQUAL(read<std::string>({"-v\"foobar\""}), "foobar");
  CAF_CHECK_EQUAL(read<std::string>({"-vfoobar"}), "foobar");
  CAF_CHECK_EQUAL(read<std::string>({"--value=\"'abc'\""}), "'abc'");
  CAF_CHECK_EQUAL(read<std::string>({"--value='abc'"}), "'abc'");
  CAF_CHECK_EQUAL(read<std::string>({"-v", "\"'abc'\""}), "'abc'");
  CAF_CHECK_EQUAL(read<std::string>({"-v", "'abc'"}), "'abc'");
  CAF_CHECK_EQUAL(read<std::string>({"-v'abc'"}), "'abc'");
  CAF_CHECK_EQUAL(read<std::string>({"--value=\"123\""}), "123");
  CAF_CHECK_EQUAL(read<std::string>({"--value=123"}), "123");
  CAF_CHECK_EQUAL(read<std::string>({"-v", "\"123\""}), "123");
  CAF_CHECK_EQUAL(read<std::string>({"-v", "123"}), "123");
  CAF_CHECK_EQUAL(read<std::string>({"-v123"}), "123");
}

CAF_TEST(flat CLI options) {
  key = "foo.bar";
  opts.add<std::string>("?foo", "bar,b", "some value");
  CAF_CHECK(opts.begin()->has_flat_cli_name());
  CAF_CHECK_EQUAL(read<std::string>({"-b", "foobar"}), "foobar");
  CAF_CHECK_EQUAL(read<std::string>({"--bar=foobar"}), "foobar");
  CAF_CHECK_EQUAL(read<std::string>({"--foo.bar=foobar"}), "foobar");
}

CAF_TEST(flat CLI parsing with nested categories) {
  key = "foo.goo.bar";
  opts.add<std::string>("?foo.goo", "bar,b", "some value");
  CAF_CHECK(opts.begin()->has_flat_cli_name());
  CAF_CHECK_EQUAL(read<std::string>({"-b", "foobar"}), "foobar");
  CAF_CHECK_EQUAL(read<std::string>({"--bar=foobar"}), "foobar");
  CAF_CHECK_EQUAL(read<std::string>({"--foo.goo.bar=foobar"}), "foobar");
}

CAF_TEST(square brackets are optional on the command line) {
  using int_list = std::vector<int>;
  opts.add<int_list>("global", "bar,b", "some list");
  CAF_CHECK_EQUAL(read<int_list>({"--bar=[1])"}), int_list({1}));
  CAF_CHECK_EQUAL(read<int_list>({"--bar=[1,])"}), int_list({1}));
  CAF_CHECK_EQUAL(read<int_list>({"--bar=[ 1 , ])"}), int_list({1}));
  CAF_CHECK_EQUAL(read<int_list>({"--bar=[1,2])"}), int_list({1, 2}));
  CAF_CHECK_EQUAL(read<int_list>({"--bar=[1, 2, 3])"}), int_list({1, 2, 3}));
  CAF_CHECK_EQUAL(read<int_list>({"--bar=[1, 2, 3, ])"}), int_list({1, 2, 3}));
  CAF_CHECK_EQUAL(read<int_list>({"--bar=1)"}), int_list({1}));
  CAF_CHECK_EQUAL(read<int_list>({"--bar=1,2,3)"}), int_list({1, 2, 3}));
  CAF_CHECK_EQUAL(read<int_list>({"--bar=1, 2 , 3 , )"}), int_list({1, 2, 3}));
}

#define SUBTEST(msg)                                                           \
  opts.clear();                                                                \
  if (true)

CAF_TEST(CLI arguments override defaults) {
  using int_list = std::vector<int>;
  using string_list = std::vector<std::string>;
  SUBTEST("with ref syncing") {
    settings cfg;
    int_list ints;
    string_list strings;
    CAF_MESSAGE("add --foo and --bar options");
    opts.add(strings, "global", "foo,f", "some list");
    opts.add(ints, "global", "bar,b", "some list");
    CAF_MESSAGE("test integer lists");
    ints = int_list{1, 2, 3};
    cfg["bar"] = config_value{ints};
    CAF_CHECK_EQUAL(get<int_list>(cfg, "bar"), int_list({1, 2, 3}));
    CAF_CHECK_EQUAL(read<int_list>(cfg, {"--bar=[10, 20, 30])"}), none);
    CAF_CHECK_EQUAL(ints, int_list({10, 20, 30}));
    CAF_CHECK_EQUAL(get<int_list>(cfg, "bar"), int_list({10, 20, 30}));
    CAF_MESSAGE("test string lists");
    strings = string_list{"one", "two", "three"};
    cfg["foo"] = config_value{strings};
    CAF_CHECK_EQUAL(get<string_list>(cfg, "foo"),
                    string_list({"one", "two", "three"}));
    CAF_CHECK_EQUAL(read<string_list>(cfg, {"--foo=[hello, world])"}), none);
    CAF_CHECK_EQUAL(strings, string_list({"hello", "world"}));
    CAF_CHECK_EQUAL(get<string_list>(cfg, "foo"),
                    string_list({"hello", "world"}));
  }
  SUBTEST("without ref syncing") {
    settings cfg;
    CAF_MESSAGE("add --foo and --bar options");
    opts.add<string_list>("global", "foo,f", "some list");
    opts.add<int_list>("global", "bar,b", "some list");
    CAF_MESSAGE("test integer lists");
    cfg["bar"] = config_value{int_list{1, 2, 3}};
    CAF_CHECK_EQUAL(get<int_list>(cfg, "bar"), int_list({1, 2, 3}));
    CAF_CHECK_EQUAL(read<int_list>(cfg, {"--bar=[10, 20, 30])"}), none);
    CAF_CHECK_EQUAL(get<int_list>(cfg, "bar"), int_list({10, 20, 30}));
    CAF_MESSAGE("test string lists");
    cfg["foo"] = config_value{string_list{"one", "two", "three"}};
    CAF_CHECK_EQUAL(get<string_list>(cfg, "foo"),
                    string_list({"one", "two", "three"}));
    CAF_CHECK_EQUAL(read<string_list>(cfg, {"--foo=[hello, world])"}), none);
    CAF_CHECK_EQUAL(get<string_list>(cfg, "foo"),
                    string_list({"hello", "world"}));
  }
}

CAF_TEST_FIXTURE_SCOPE_END()
