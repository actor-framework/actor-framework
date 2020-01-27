/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#define CAF_SUITE settings

#include "caf/settings.hpp"

#include "caf/test/dsl.hpp"

#include <string>

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

const config_value& unpack(const settings& x, string_view key) {
  auto i = x.find(key);
  if (i == x.end())
    CAF_FAIL("key not found in dictionary: " << key);
  return i->second;
}

template <class... Ts>
const config_value& unpack(const settings& x, string_view key,
                           const char* next_key, Ts... keys) {
  auto i = x.find(key);
  if (i == x.end())
    CAF_FAIL("key not found in dictionary: " << key);
  if (!holds_alternative<settings>(i->second))
    CAF_FAIL("value is not a dictionary: " << key);
  return unpack(get<settings>(i->second), {next_key, strlen(next_key)},
                keys...);
}

struct foobar {
  int foo = 0;
  int bar = 0;
};

} // namespace

namespace caf {

// Enable users to configure foobar's like this:
// my-value {
//   foo = 42
//   bar = 23
// }
template <>
struct config_value_access<foobar> {
  static bool is(const config_value& x) {
    auto dict = caf::get_if<config_value::dictionary>(&x);
    if (dict != nullptr) {
      return caf::get_if<int>(dict, "foo") != none
             && caf::get_if<int>(dict, "bar") != none;
    }
    return false;
  }

  static optional<foobar> get_if(const config_value* x) {
    foobar result;
    if (!is(*x))
      return none;
    const auto& dict = caf::get<config_value::dictionary>(*x);
    result.foo = caf::get<int>(dict, "foo");
    result.bar = caf::get<int>(dict, "bar");
    return result;
  }

  static foobar get(const config_value& x) {
    auto result = get_if(&x);
    if (!result)
      CAF_RAISE_ERROR("invalid type found");
    return std::move(*result);
  }
};

} // namespace caf

CAF_TEST_FIXTURE_SCOPE(settings_tests, fixture)

CAF_TEST(put) {
  put(x, "foo", "bar");
  put(x, "logger.console", "none");
  put(x, "one.two.three", "four");
  CAF_CHECK_EQUAL(x.size(), 3u);
  CAF_CHECK(x.contains("foo"));
  CAF_CHECK(x.contains("logger"));
  CAF_CHECK(x.contains("one"));
  CAF_CHECK_EQUAL(unpack(x, "foo"), "bar"s);
  CAF_CHECK_EQUAL(unpack(x, "logger", "console"), "none"s);
  CAF_CHECK_EQUAL(unpack(x, "one", "two", "three"), "four"s);
  put(x, "logger.console", "trace");
  CAF_CHECK_EQUAL(unpack(x, "logger", "console"), "trace"s);
}

CAF_TEST(put missing) {
  put_missing(x, "foo", "bar");
  put_missing(x, "logger.console", "none");
  put_missing(x, "one.two.three", "four");
  CAF_CHECK_EQUAL(x.size(), 3u);
  CAF_CHECK(x.contains("foo"));
  CAF_CHECK(x.contains("logger"));
  CAF_CHECK(x.contains("one"));
  CAF_CHECK_EQUAL(unpack(x, "foo"), "bar"s);
  CAF_CHECK_EQUAL(unpack(x, "logger", "console"), "none"s);
  CAF_CHECK_EQUAL(unpack(x, "one", "two", "three"), "four"s);
  put_missing(x, "logger.console", "trace");
  CAF_CHECK_EQUAL(unpack(x, "logger", "console"), "none"s);
}

CAF_TEST(put list) {
  put_list(x, "integers").emplace_back(42);
  CAF_CHECK(x.contains("integers"));
  CAF_CHECK_EQUAL(unpack(x, "integers"), make_config_value_list(42));
  put_list(x, "foo.bar").emplace_back("str");
  CAF_CHECK_EQUAL(unpack(x, "foo", "bar"), make_config_value_list("str"));
  put_list(x, "one.two.three").emplace_back(4);
  CAF_CHECK_EQUAL(unpack(x, "one", "two", "three"), make_config_value_list(4));
}

CAF_TEST(put dictionary) {
  put_dictionary(x, "logger").emplace("console", "none");
  CAF_CHECK(x.contains("logger"));
  CAF_CHECK_EQUAL(unpack(x, "logger", "console"), "none"s);
  put_dictionary(x, "foo.bar").emplace("value", 42);
  CAF_CHECK_EQUAL(unpack(x, "foo", "bar", "value"), 42);
  put_dictionary(x, "one.two.three").emplace("four", "five");
  CAF_CHECK_EQUAL(unpack(x, "one", "two", "three", "four"), "five"s);
}

CAF_TEST(get and get_if) {
  fill();
  CAF_CHECK(get_if(&x, "hello") != nullptr);
  CAF_CHECK(get<std::string>(x, "hello") == "world"s);
  CAF_CHECK(get_if(&x, "logger.console") != nullptr);
  CAF_CHECK(get_if<std::string>(&x, "logger.console") != nullptr);
  CAF_CHECK(get<std::string>(x, "logger.console") == "none"s);
  CAF_CHECK(get_if(&x, "one.two.three") != nullptr);
  CAF_CHECK(get_if<std::string>(&x, "one.two.three") == nullptr);
  CAF_REQUIRE(get_if<int>(&x, "one.two.three") != none);
  CAF_CHECK(get<int>(x, "one.two.three") == 4);
}

CAF_TEST(get_or) {
  fill();
  CAF_CHECK_EQUAL(get_or(x, "hello", "nobody"), "world"s);
  CAF_CHECK_EQUAL(get_or(x, "goodbye", "nobody"), "nobody"s);
}

CAF_TEST(custom type) {
  put(x, "my-value.foo", 42);
  put(x, "my-value.bar", 24);
  CAF_REQUIRE(holds_alternative<foobar>(x, "my-value"));
  CAF_REQUIRE(get_if<foobar>(&x, "my-value") != caf::none);
  auto fb = get<foobar>(x, "my-value");
  CAF_CHECK_EQUAL(fb.foo, 42);
  CAF_CHECK_EQUAL(fb.bar, 24);
}

CAF_TEST_FIXTURE_SCOPE_END()
