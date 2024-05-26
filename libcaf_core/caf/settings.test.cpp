// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/settings.hpp"

#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/detail/config_consumer.hpp"
#include "caf/detail/parser/read_config.hpp"
#include "caf/init_global_meta_objects.hpp"
#include "caf/none.hpp"

#include <string>

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

config_value unpack(const settings& x, std::string_view key) {
  if (auto i = x.find(key); i != x.end())
    return i->second;
  else
    return {};
}

template <class... Ts>
config_value unpack(const settings& x, std::string_view key,
                    const char* next_key, Ts... keys) {
  auto i = x.find(key);
  if (i == x.end())
    return {};
  if (auto ptr = get_if<settings>(std::addressof(i->second)))
    return unpack(*ptr, {next_key, strlen(next_key)}, keys...);
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

struct dummy_struct {
  int a;
  std::string b;
};

[[maybe_unused]] inline bool operator==(const dummy_struct& x,
                                        const dummy_struct& y) {
  return x.a == y.a && x.b == y.b;
}

template <class Inspector>
bool inspect(Inspector& f, dummy_struct& x) {
  return f.object(x).fields(f.field("a", x.a), f.field("b", x.b));
}

} // namespace

CAF_BEGIN_TYPE_ID_BLOCK(settings_test, caf::first_custom_type_id + 40)

  CAF_ADD_TYPE_ID(settings_test, (dummy_struct))

CAF_END_TYPE_ID_BLOCK(settings_test)

namespace {

WITH_FIXTURE(fixture) {

TEST("put") {
  put(x, "foo", "bar");
  put(x, "logger.console", config_value{"none"});
  put(x, "one.two.three", "four");
  check_eq(x.size(), 3u);
  check(x.contains("foo"));
  check(x.contains("logger"));
  check(x.contains("one"));
  check_eq(unpack(x, "foo"), config_value{"bar"s});
  check_eq(unpack(x, "logger", "console"), config_value{"none"s});
  check_eq(unpack(x, "one", "two", "three"), config_value{"four"s});
  put(x, "logger.console", "trace");
  check_eq(unpack(x, "logger", "console"), config_value{"trace"s});
}

TEST("put missing") {
  put_missing(x, "foo", "bar");
  put_missing(x, "logger.console", "none");
  put_missing(x, "one.two.three", "four");
  check_eq(x.size(), 3u);
  check(x.contains("foo"));
  check(x.contains("logger"));
  check(x.contains("one"));
  check_eq(unpack(x, "foo"), config_value{"bar"s});
  check_eq(unpack(x, "logger", "console"), config_value{"none"s});
  check_eq(unpack(x, "one", "two", "three"), config_value{"four"s});
  put_missing(x, "logger.console", "trace");
  check_eq(unpack(x, "logger", "console"), config_value{"none"s});
}

TEST("put list") {
  put_list(x, "integers").emplace_back(42);
  check(x.contains("integers"));
  check_eq(unpack(x, "integers"), make_config_value_list(42));
  put_list(x, "foo.bar").emplace_back("str");
  check_eq(unpack(x, "foo", "bar"), make_config_value_list("str"));
  put_list(x, "one.two.three").emplace_back(4);
  check_eq(unpack(x, "one", "two", "three"), make_config_value_list(4));
}

TEST("put dictionary") {
  put_dictionary(x, "logger").emplace("console", "none");
  check(x.contains("logger"));
  check_eq(unpack(x, "logger", "console"), config_value{"none"s});
  put_dictionary(x, "foo.bar").emplace("value", 42);
  check_eq(unpack(x, "foo", "bar", "value"), config_value{42});
  put_dictionary(x, "one.two.three").emplace("four", "five");
  check_eq(unpack(x, "one", "two", "three", "four"), config_value{"five"s});
}

TEST("get and get_if") {
  fill();
  check(get_if(&x, "hello") != nullptr);
  check(get<std::string>(x, "hello") == "world"s);
  check(get_if(&x, "logger.console") != nullptr);
  check(get_if<std::string>(&x, "logger.console") != nullptr);
  check(get<std::string>(x, "logger.console") == "none"s);
  check(get_if(&x, "one.two.three") != nullptr);
  check(get_if<std::string>(&x, "one.two.three") == nullptr);
  if (check(get_if<int64_t>(&x, "one.two.three") != nullptr))
    check(get<int64_t>(x, "one.two.three") == 4);
}

TEST("get_or") {
  fill();
  check_eq(get_or(x, "hello", "nobody"), "world"s);
  check_eq(get_or(x, "goodbye", "nobody"), "nobody"s);
}

TEST("custom type") {
  put(x, "my-value.foo", 42);
  put(x, "my-value.bar", 24);
  if (auto fb = get_as<foobar>(x, "my-value"); check(fb.has_value())) {
    check_eq(fb->foo, 42);
    check_eq(fb->bar, 24);
  }
}

TEST("read_config accepts the to_string output of settings") {
  fill();
  auto str = to_string(x);
  settings y;
  config_option_set dummy;
  detail::config_consumer consumer{dummy, y};
  std::string_view str_view = str;
  string_parser_state res{str_view.begin(), str_view.end()};
  detail::parser::read_config(res, consumer);
  check(res.i == res.e);
  check_eq(res.code, pec::success);
  check_eq(x, y);
}

SCENARIO("put_missing normalizes 'global' suffixes") {
  GIVEN("empty settings") {
    settings uut;
    WHEN("calling put_missing with and without 'global' suffix") {
      THEN("put_missing drops the 'global' suffix") {
        put_missing(uut, "global.foo", "bar"s);
        check_eq(get_as<std::string>(uut, "foo"), "bar"s);
        check_eq(get_as<std::string>(uut, "global.foo"), "bar"s);
      }
    }
  }
  GIVEN("settings with a value for 'foo'") {
    settings uut;
    uut["foo"] = "bar"s;
    WHEN("calling put_missing 'global.foo'") {
      THEN("the function call results in a no-op") {
        put_missing(uut, "global.foo", "baz"s);
        check_eq(get_as<std::string>(uut, "foo"), "bar"s);
        check_eq(get_as<std::string>(uut, "global.foo"), "bar"s);
      }
    }
  }
}

TEST("put and put_missing decomposes user-defined types") {
  settings uut;
  put(uut, "dummy", dummy_struct{42, "foo"});
  check_eq(get_as<int>(uut, "dummy.a"), 42);
  check_eq(get_as<std::string>(uut, "dummy.b"), "foo"s);
  uut.clear();
  put_missing(uut, "dummy", dummy_struct{23, "bar"});
  check_eq(get_as<int>(uut, "dummy.a"), 23);
  check_eq(get_as<std::string>(uut, "dummy.b"), "bar"s);
}

} // WITH_FIXTURE(fixture)

} // namespace
