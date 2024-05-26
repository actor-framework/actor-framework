// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/config_option_set.hpp"

#include "caf/test/approx.hpp"
#include "caf/test/test.hpp"

#include "caf/log/test.hpp"
#include "caf/settings.hpp"

#include <map>
#include <string>
#include <vector>

using std::string;
using std::vector;

using namespace caf;

namespace {

struct foobar {
  std::string foo;
  std::string bar;
};

bool operator==(const foobar& x, const foobar& y) {
  return x.foo == y.foo && x.bar == y.bar;
}

template <class Inspector>
bool inspect(Inspector& f, foobar& x) {
  return f.object(x).fields(f.field("foo", x.foo), f.field("bar", x.bar));
}

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
      return {res.first};
    else if (auto x = get_as<T>(cfg, key))
      return {std::move(*x)};
    else
      return {sec::invalid_argument};
  }

  std::string key = "value";
};

WITH_FIXTURE(fixture) {

TEST("lookup") {
  opts.add<int>("opt1,1", "test option 1")
    .add<float>("test", "opt2,2", "test option 2")
    .add<bool>("test", "flag,fl3", "test flag");
  check_eq(opts.size(), 3u);
  log::test::debug("lookup by long name");
  check_ne(opts.cli_long_name_lookup("opt1"), nullptr);
  check_ne(opts.cli_long_name_lookup("test.opt2"), nullptr);
  check_ne(opts.cli_long_name_lookup("test.flag"), nullptr);
  log::test::debug("lookup by short name");
  check_ne(opts.cli_short_name_lookup('1'), nullptr);
  check_ne(opts.cli_short_name_lookup('2'), nullptr);
  check_ne(opts.cli_short_name_lookup('f'), nullptr);
  check_ne(opts.cli_short_name_lookup('l'), nullptr);
  check_ne(opts.cli_short_name_lookup('3'), nullptr);
}

TEST("help text") {
  std::string expected = R"__(global options:
  (-1|--opt1) <int32_t>    : test option 1
  --opt2=<float>           : test option 2
  (-f|-l|-1|--flag)        : test flag 1

test options:
  --test.opt3=<float>      : test option 3
  (-4|--test.opt4) <float> : test option 4
  (-f|-l|-2|--test.flag)   : test flag 2

)__";
  opts.add<int>("opt1,1", "test option 1")
    .add<float>("opt2", "test option 2")
    .add<bool>("flag,fl1", "test flag 1")
    .add<float>("test", "opt3", "test option 3")
    .add<float>("test", "opt4,4", "test option 4")
    .add<bool>("test", "flag,fl2", "test flag 2");
  check_eq(opts.size(), 6u);
  check_eq(expected, opts.help_text());
}

TEST("parse with ref syncing") {
  using ls = vector<string>;     // list of strings
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
  SECTION("parse valid arguments with pec::success") {
    vector<string> args{"-i42",
                        "-f",
                        "1e2",
                        "-shello",
                        "--bar.l=[\"hello\", \"world\"]",
                        "-d",
                        "{a=\"a\",b=\"b\"}",
                        "-b"};
    log::test::debug("parse arguments");
    auto res = opts.parse(cfg, args);
    check_eq(res.first, pec::success);
    if (res.second != args.end())
      fail("parser stopped at: {}", *res.second);
    log::test::debug("verify referenced values");
    check_eq(foo_i, 42);
    check_eq(foo_f, caf::test::approx{1e2});
    check_eq(foo_b, true);
    check_eq(bar_s, "hello");
    check_eq(bar_l, ls({"hello", "world"}));
    check_eq(bar_d, ds({{"a", "a"}, {"b", "b"}}));
    log::test::debug("verify dictionary content");
    check_eq(get_as<int>(cfg, "foo.i"), 42);
  }
  SECTION("parse invalid arguments with error") {
    auto check_parse_options
      = [this](const config_option_set::parse_result& res,
               const config_option_set::parse_result cmp) {
          check_eq(res.first, cmp.first);
          check_eq(res.second, cmp.second);
        };
    vector<string> long_option_no_arg{"--"};
    check_parse_options(
      opts.parse(cfg, long_option_no_arg),
      config_option_set::parse_result{pec::success, long_option_no_arg.end()});
    vector<string> short_option_no_arg{"-"};
    check_parse_options(opts.parse(cfg, short_option_no_arg),
                        config_option_set::parse_result{
                          pec::not_an_option, short_option_no_arg.begin()});
    vector<string> invalid_option_format{"foo"};
    check_parse_options(opts.parse(cfg, invalid_option_format),
                        config_option_set::parse_result{
                          pec::not_an_option, invalid_option_format.begin()});
    SECTION("single argument") {
      vector<string> long_option_with_wrong_type{
        "--bar.l={\"hello\", \"world\"}"};
      check_parse_options(
        opts.parse(cfg, long_option_with_wrong_type),
        config_option_set::parse_result{pec::invalid_argument,
                                        long_option_with_wrong_type.begin()});
      vector<string> short_option_missing_value{"-i"};
      check_parse_options(
        opts.parse(cfg, short_option_missing_value),
        config_option_set::parse_result{pec::missing_argument,
                                        short_option_missing_value.end()});
      vector<string> short_option_wrong_value_type{"-ihello"};
      check_parse_options(
        opts.parse(cfg, short_option_wrong_value_type),
        config_option_set::parse_result{pec::invalid_argument,
                                        short_option_wrong_value_type.begin()});
      vector<string> short_option_invalid_flag{"-bhello"};
      check_parse_options(
        opts.parse(cfg, short_option_invalid_flag),
        config_option_set::parse_result{pec::invalid_argument,
                                        short_option_invalid_flag.begin()});
      vector<string> short_option_missing_flag{"-k"};
      check_parse_options(
        opts.parse(cfg, short_option_missing_flag),
        config_option_set::parse_result{pec::not_an_option,
                                        short_option_missing_flag.begin()});
      vector<string> long_option_missing_flag{"--k"};
      check_parse_options(
        opts.parse(cfg, long_option_missing_flag),
        config_option_set::parse_result{pec::not_an_option,
                                        long_option_missing_flag.begin()});
      vector<string> long_option_missing_value{"--bar.l"};
      check_parse_options(
        opts.parse(cfg, long_option_missing_value),
        config_option_set::parse_result{pec::missing_argument,
                                        long_option_missing_value.end()});
    }
    SECTION("multiple arguments") {
      vector<string> long_option_wrong_value_type{"--bar.l",
                                                  "{\"hello\", \"world\"}"};
      check_parse_options(
        opts.parse(cfg, long_option_wrong_value_type),
        config_option_set::parse_result{pec::invalid_argument,
                                        long_option_wrong_value_type.begin()});
      vector<string> short_option_wrong_value_type{"-i", "hello"};
      check_parse_options(
        opts.parse(cfg, short_option_wrong_value_type),
        config_option_set::parse_result{pec::invalid_argument,
                                        short_option_wrong_value_type.begin()});
      vector<string> short_option_missing_value{"-i", ""};
      check_parse_options(
        opts.parse(cfg, short_option_missing_value),
        config_option_set::parse_result{pec::missing_argument,
                                        short_option_missing_value.begin()});
    }
  }
}

TEST("long format for flags") {
  auto foo_b = false;
  opts.add<bool>(foo_b, "foo", "b,b", "");
  settings cfg;
  vector<string> args{"--foo.b"};
  auto res = opts.parse(cfg, args);
  check_eq(res.first, pec::success);
  if (res.second != args.end())
    fail("parser stopped at: {}", *res.second);
  check_eq(foo_b, true);
}

TEST("string parameters") {
  opts.add<std::string>("value,v", "some value");
  check_eq(read<std::string>({"--value=foobar"}), "foobar");
  check_eq(read<std::string>({"--value", "foobar"}), "foobar");
  check_eq(read<std::string>({"-v", "foobar"}), "foobar");
  check_eq(read<std::string>({"-vfoobar"}), "foobar");
}

TEST("flat CLI options") {
  key = "foo.bar";
  opts.add<std::string>("?foo", "bar,b", "some value");
  check(opts.begin()->has_flat_cli_name());
  check_eq(read<std::string>({"-b", "foobar"}), "foobar");
  check_eq(read<std::string>({"-bfoobar"}), "foobar");
  check_eq(read<std::string>({"--bar=foobar"}), "foobar");
  check_eq(read<std::string>({"--foo.bar=foobar"}), "foobar");
  check_eq(read<std::string>({"--bar", "foobar"}), "foobar");
  check_eq(read<std::string>({"--foo.bar", "foobar"}), "foobar");
}

TEST("flat CLI parsing with nested categories") {
  key = "foo.goo.bar";
  opts.add<std::string>("?foo.goo", "bar,b", "some value");
  check(opts.begin()->has_flat_cli_name());
  check_eq(read<std::string>({"-b", "foobar"}), "foobar");
  check_eq(read<std::string>({"-bfoobar"}), "foobar");
  check_eq(read<std::string>({"--bar=foobar"}), "foobar");
  check_eq(read<std::string>({"--foo.goo.bar=foobar"}), "foobar");
  check_eq(read<std::string>({"--bar", "foobar"}), "foobar");
  check_eq(read<std::string>({"--foo.goo.bar", "foobar"}), "foobar");
}

TEST("square brackets are optional on the command line") {
  using int_list = std::vector<int>;
  opts.add<int_list>("global", "value,v", "some list");
  check_eq(read<int_list>({"--value=[1]"}), int_list({1}));
  check_eq(read<int_list>({"--value=[1,]"}), int_list({1}));
  check_eq(read<int_list>({"--value=[ 1 , ]"}), int_list({1}));
  check_eq(read<int_list>({"--value=[1,2]"}), int_list({1, 2}));
  check_eq(read<int_list>({"--value=[1, 2, 3]"}), int_list({1, 2, 3}));
  check_eq(read<int_list>({"--value=[1, 2, 3, ]"}), int_list({1, 2, 3}));
  check_eq(read<int_list>({"--value=1"}), int_list({1}));
  check_eq(read<int_list>({"--value=1,2,3"}), int_list({1, 2, 3}));
  check_eq(read<int_list>({"--value=1, 2 , 3 , "}), int_list({1, 2, 3}));
  check_eq(read<int_list>({"--value", "[1]"}), int_list({1}));
  check_eq(read<int_list>({"--value", "[1,]"}), int_list({1}));
  check_eq(read<int_list>({"--value", "[ 1 , ]"}), int_list({1}));
  check_eq(read<int_list>({"--value", "[1,2]"}), int_list({1, 2}));
  check_eq(read<int_list>({"--value", "[1, 2, 3]"}), int_list({1, 2, 3}));
  check_eq(read<int_list>({"--value", "[1, 2, 3, ]"}), int_list({1, 2, 3}));
  check_eq(read<int_list>({"--value", "1"}), int_list({1}));
  check_eq(read<int_list>({"--value", "1,2,3"}), int_list({1, 2, 3}));
  check_eq(read<int_list>({"--value", "1, 2 , 3 , "}), int_list({1, 2, 3}));
  check_eq(read<int_list>({"-v", "[1]"}), int_list({1}));
  check_eq(read<int_list>({"-v", "[1,]"}), int_list({1}));
  check_eq(read<int_list>({"-v", "[ 1 , ]"}), int_list({1}));
  check_eq(read<int_list>({"-v", "[1,2]"}), int_list({1, 2}));
  check_eq(read<int_list>({"-v", "[1, 2, 3]"}), int_list({1, 2, 3}));
  check_eq(read<int_list>({"-v", "[1, 2, 3, ]"}), int_list({1, 2, 3}));
  check_eq(read<int_list>({"-v", "1"}), int_list({1}));
  check_eq(read<int_list>({"-v", "1,2,3"}), int_list({1, 2, 3}));
  check_eq(read<int_list>({"-v", "1, 2 , 3 , "}), int_list({1, 2, 3}));
}

#define SUBTEST(msg)                                                           \
  opts.clear();                                                                \
  if (true)

TEST("CLI arguments override defaults") {
  using int_list = std::vector<int>;
  using string_list = std::vector<std::string>;
  SUBTEST("with ref syncing") {
    settings cfg;
    int_list ints;
    string_list strings;
    log::test::debug("add --foo and --bar options");
    opts.add(strings, "global", "foo,f", "some list");
    opts.add(ints, "global", "bar,b", "some list");
    log::test::debug("test integer lists");
    ints = int_list{1, 2, 3};
    cfg["bar"] = config_value{ints};
    check_eq(get_as<int_list>(cfg, "bar"), int_list({1, 2, 3}));
    check_eq(read<int_list>(cfg, {"--bar=[10, 20, 30]"}), none);
    check_eq(ints, int_list({10, 20, 30}));
    check_eq(get_as<int_list>(cfg, "bar"), int_list({10, 20, 30}));
    log::test::debug("test string lists");
    strings = string_list{"one", "two", "three"};
    cfg["foo"] = config_value{strings};
    check_eq(get_as<string_list>(cfg, "foo"),
             string_list({"one", "two", "three"}));
    check_eq(read<string_list>(cfg, {R"_(--foo=["hello", "world"])_"}), none);
    check_eq(strings, string_list({"hello", "world"}));
    check_eq(get_as<string_list>(cfg, "foo"), string_list({"hello", "world"}));
  }
  SUBTEST("without ref syncing") {
    settings cfg;
    log::test::debug("add --foo and --bar options");
    opts.add<string_list>("global", "foo,f", "some list");
    opts.add<int_list>("global", "bar,b", "some list");
    log::test::debug("test integer lists");
    cfg["bar"] = config_value{int_list{1, 2, 3}};
    check_eq(get_as<int_list>(cfg, "bar"), int_list({1, 2, 3}));
    check_eq(read<int_list>(cfg, {"--bar=[10, 20, 30]"}), none);
    check_eq(get_as<int_list>(cfg, "bar"), int_list({10, 20, 30}));
    log::test::debug("test string lists");
    cfg["foo"] = config_value{string_list{"one", "two", "three"}};
    check_eq(get_as<string_list>(cfg, "foo"),
             string_list({"one", "two", "three"}));
    check_eq(read<string_list>(cfg, {R"_(--foo=["hello", "world"])_"}), none);
    check_eq(get_as<string_list>(cfg, "foo"), string_list({"hello", "world"}));
  }
}

TEST("CLI arguments may use custom types") {
  settings cfg;
  opts.add<foobar>("global", "foobar,f", "test option");
  check_eq(read<foobar>(cfg, {"-f{foo=\"hello\",bar=\"world\"}"}), none);
  if (auto fb = get_as<foobar>(cfg, "foobar"); check(static_cast<bool>(fb))) {
    auto want = foobar{"hello", "world"};
    check_eq(*fb, want);
  }
}

} // WITH_FIXTURE(fixture)

} // namespace
