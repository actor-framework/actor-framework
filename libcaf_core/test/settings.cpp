// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE settings

#include "caf/settings.hpp"

#include "core-test.hpp"

#include <string>

#include "caf/detail/config_consumer.hpp"
#include "caf/detail/parser/read_config.hpp"
#include "caf/none.hpp"
#include "caf/optional.hpp"

using namespace std::string_literals;

using namespace caf;

namespace {

struct fixture {
  settings x;

  void fill() {
    x["hello"] = "world";
    x["one"].as_dictionary()["two"].as_dictionary()["three"] = 4;
    auto& logger = x["logger"].as_dictionary();
    logger["component-blacklist"] = make_config_value_list("caf");
    logger["console"] = "none";
    logger["console-format"] = "%m";
    logger["console-verbosity"] = "trace";
    logger["file-format"] = "%r %c %p %a %t %C %M %F:%L %m%n";
    logger["inline-output"] = false;
    auto& middleman = x["middleman"].as_dictionary();
    middleman["app-identifiers"] = make_config_value_list("generic-caf-app");
    middleman["enable-automatic-connections"] = false;
    middleman["heartbeat-interval"] = 0;
    middleman["max-consecutive-reads"] = 50;
    middleman["workers"] = 3;
    auto& stream = x["stream"].as_dictionary();
    stream["credit-round-interval"] = timespan{10000000}; // 10ms;
    stream["desired-batch-complexity"] = timespan{50000}; // 50us;
    stream["max-batch-delay"] = timespan{5000000};        // 5ms;
  }
};

config_value unpack(const settings& x, string_view key) {
  if (auto i = x.find(key); i != x.end())
    return i->second;
  else
    return {};
}

template <class... Ts>
config_value
unpack(const settings& x, string_view key, const char* next_key, Ts... keys) {
  if (auto i = x.find(key); i == x.end())
    return {};
  else if (auto ptr = get_if<settings>(std::addressof(i->second)))
    return unpack(*ptr, {next_key, strlen(next_key)}, keys...);
  else
    return {};
}

struct foobar {
  int foo = 0;
  int bar = 0;
};

template <class Inspector>
bool inspect(Inspector& f, foobar& x) {
  return f.object(x).fields(f.field("foo", x.foo), f.field("bar", x.bar));
}

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

CAF_TEST(put) {
  put(x, "foo", "bar");
  put(x, "logger.console", "none");
  put(x, "one.two.three", "four");
  CHECK_EQ(x.size(), 3u);
  CHECK(x.contains("foo"));
  CHECK(x.contains("logger"));
  CHECK(x.contains("one"));
  CHECK_EQ(unpack(x, "foo"), config_value{"bar"s});
  CHECK_EQ(unpack(x, "logger", "console"), config_value{"none"s});
  CHECK_EQ(unpack(x, "one", "two", "three"), config_value{"four"s});
  put(x, "logger.console", "trace");
  CHECK_EQ(unpack(x, "logger", "console"), config_value{"trace"s});
}

CAF_TEST(put missing) {
  put_missing(x, "foo", "bar");
  put_missing(x, "logger.console", "none");
  put_missing(x, "one.two.three", "four");
  CHECK_EQ(x.size(), 3u);
  CHECK(x.contains("foo"));
  CHECK(x.contains("logger"));
  CHECK(x.contains("one"));
  CHECK_EQ(unpack(x, "foo"), config_value{"bar"s});
  CHECK_EQ(unpack(x, "logger", "console"), config_value{"none"s});
  CHECK_EQ(unpack(x, "one", "two", "three"), config_value{"four"s});
  put_missing(x, "logger.console", "trace");
  CHECK_EQ(unpack(x, "logger", "console"), config_value{"none"s});
}

CAF_TEST(put list) {
  put_list(x, "integers").emplace_back(42);
  CHECK(x.contains("integers"));
  CHECK_EQ(unpack(x, "integers"), make_config_value_list(42));
  put_list(x, "foo.bar").emplace_back("str");
  CHECK_EQ(unpack(x, "foo", "bar"), make_config_value_list("str"));
  put_list(x, "one.two.three").emplace_back(4);
  CHECK_EQ(unpack(x, "one", "two", "three"), make_config_value_list(4));
}

CAF_TEST(put dictionary) {
  put_dictionary(x, "logger").emplace("console", "none");
  CHECK(x.contains("logger"));
  CHECK_EQ(unpack(x, "logger", "console"), config_value{"none"s});
  put_dictionary(x, "foo.bar").emplace("value", 42);
  CHECK_EQ(unpack(x, "foo", "bar", "value"), config_value{42});
  put_dictionary(x, "one.two.three").emplace("four", "five");
  CHECK_EQ(unpack(x, "one", "two", "three", "four"), config_value{"five"s});
}

CAF_TEST(get and get_if) {
  fill();
  CHECK(get_if(&x, "hello") != nullptr);
  CHECK(get<std::string>(x, "hello") == "world"s);
  CHECK(get_if(&x, "logger.console") != nullptr);
  CHECK(get_if<std::string>(&x, "logger.console") != nullptr);
  CHECK(get<std::string>(x, "logger.console") == "none"s);
  CHECK(get_if(&x, "one.two.three") != nullptr);
  CHECK(get_if<std::string>(&x, "one.two.three") == nullptr);
  if (CHECK(get_if<int64_t>(&x, "one.two.three") != nullptr))
    CHECK(get<int64_t>(x, "one.two.three") == 4);
}

CAF_TEST(get_or) {
  fill();
  CHECK_EQ(get_or(x, "hello", "nobody"), "world"s);
  CHECK_EQ(get_or(x, "goodbye", "nobody"), "nobody"s);
}

CAF_TEST(custom type) {
  put(x, "my-value.foo", 42);
  put(x, "my-value.bar", 24);
  if (auto fb = get_as<foobar>(x, "my-value"); CHECK(fb)) {
    CHECK_EQ(fb->foo, 42);
    CHECK_EQ(fb->bar, 24);
  }
}

CAF_TEST(read_config accepts the to_string output of settings) {
  fill();
  auto str = to_string(x);
  settings y;
  config_option_set dummy;
  detail::config_consumer consumer{dummy, y};
  string_view str_view = str;
  string_parser_state res{str_view.begin(), str_view.end()};
  detail::parser::read_config(res, consumer);
  CHECK(res.i == res.e);
  CHECK_EQ(res.code, pec::success);
  CHECK_EQ(x, y);
}

SCENARIO("put_missing normalizes 'global' suffixes") {
  GIVEN("empty settings") {
    settings uut;
    WHEN("calling put_missing with and without 'global' suffix") {
      THEN("put_missing drops the 'global' suffix") {
        put_missing(uut, "global.foo", "bar"s);
        CHECK_EQ(get_as<std::string>(uut, "foo"), "bar"s);
        CHECK_EQ(get_as<std::string>(uut, "global.foo"), "bar"s);
      }
    }
  }
  GIVEN("settings with a value for 'foo'") {
    settings uut;
    uut["foo"] = "bar"s;
    WHEN("calling put_missing 'global.foo'") {
      THEN("the function call results in a no-op") {
        put_missing(uut, "global.foo", "baz"s);
        CHECK_EQ(get_as<std::string>(uut, "foo"), "bar"s);
        CHECK_EQ(get_as<std::string>(uut, "global.foo"), "bar"s);
      }
    }
  }
}

END_FIXTURE_SCOPE()
