// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE actor_system_config

#include "caf/actor_system_config.hpp"

#include "core-test.hpp"

#include <deque>
#include <list>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace caf;
using namespace caf::literals;

using namespace std::string_literals;

namespace {

timespan operator"" _ms(unsigned long long x) {
  return std::chrono::duration_cast<timespan>(std::chrono::milliseconds(x));
}

uri operator"" _u(const char* str, size_t size) {
  return unbox(make_uri(std::string_view{str, size}));
}

using string_list = std::vector<std::string>;

struct config : actor_system_config {
  config_option_adder options(std::string_view category) {
    return opt_group{custom_options_, category};
  }

  void clear() {
    content.clear();
    remainder.clear();
  }
};

struct fixture {
  config cfg;

  config_option_adder options(std::string_view category) {
    return cfg.options(category);
  }

  void parse(const char* file_content, string_list args = {}) {
    cfg.clear();
    std::istringstream conf{file_content};
    if (auto err = cfg.parse(std::move(args), conf))
      CAF_FAIL("parse() failed: " << err);
  }
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

CAF_TEST(parsing - without CLI arguments) {
  auto text = "foo{\nbar=\"hello\"}";
  options("?foo").add<std::string>("bar,b", "some string parameter");
  parse(text);
  CHECK(cfg.remainder.empty());
  CHECK_EQ(get_or(cfg, "foo.bar", ""), "hello");
  auto [argc, argv] = cfg.c_args_remainder();
  CAF_REQUIRE_EQUAL(argc, 1);
  CHECK_EQ(argv[0], cfg.program_name);
}

CAF_TEST(parsing - without CLI cfg.remainder) {
  auto text = "foo{\nbar=\"hello\"}";
  options("?foo").add<std::string>("bar,b", "some string parameter");
  MESSAGE("CLI long name");
  parse(text, {"--foo.bar=test"});
  CHECK(cfg.remainder.empty());
  CHECK_EQ(get_or(cfg, "foo.bar", ""), "test");
  MESSAGE("CLI abbreviated long name");
  parse(text, {"--bar=test"});
  CHECK(cfg.remainder.empty());
  CHECK_EQ(get_or(cfg, "foo.bar", ""), "test");
  MESSAGE("CLI short name");
  parse(text, {"-b", "test"});
  CHECK(cfg.remainder.empty());
  CHECK_EQ(get_or(cfg, "foo.bar", ""), "test");
  MESSAGE("CLI short name without whitespace");
  parse(text, {"-btest"});
  CHECK(cfg.remainder.empty());
  CHECK_EQ(get_or(cfg, "foo.bar", ""), "test");
}

CAF_TEST(parsing - with CLI cfg.remainder) {
  auto text = "foo{\nbar=\"hello\"}";
  options("?foo").add<std::string>("bar,b", "some string parameter");
  parse(text, {"-b", "test", "hello", "world"});
  CAF_REQUIRE_EQUAL(cfg.remainder.size(), 2u);
  CHECK_EQ(get_or(cfg, "foo.bar", ""), "test");
  CHECK_EQ(cfg.remainder, string_list({"hello", "world"}));
  auto [argc, argv] = cfg.c_args_remainder();
  CAF_REQUIRE_EQUAL(argc, 3);
  CHECK_EQ(argv[0], cfg.program_name);
  CHECK_EQ(argv[1], cfg.remainder[0]);
  CHECK_EQ(argv[2], cfg.remainder[1]);
}

CAF_TEST(file input overrides defaults but CLI args always win) {
  const char* file_input = R"__(
    group1 {
      arg1 = 'foobar'
    }
    group2 {
      arg1 = 'hello world'
      arg2 = 2
    }
  )__";
  struct grp {
    std::string arg1 = "default";
    int arg2 = 42;
  };
  grp grp1;
  grp grp2;
  config_option_adder{cfg.custom_options(), "group1"}
    .add(grp1.arg1, "arg1", "")
    .add(grp1.arg2, "arg2", "");
  config_option_adder{cfg.custom_options(), "group2"}
    .add(grp2.arg1, "arg1", "")
    .add(grp2.arg2, "arg2", "");
  string_list args{"--group1.arg2=123", "--group2.arg1=bye"};
  std::istringstream input{file_input};
  auto err = cfg.parse(std::move(args), input);
  CHECK_EQ(err, error{});
  CHECK_EQ(grp1.arg1, "foobar");
  CHECK_EQ(grp1.arg2, 123);
  CHECK_EQ(grp2.arg1, "bye");
  CHECK_EQ(grp2.arg2, 2);
  settings res;
  put(res, "group1.arg1", "foobar");
  put(res, "group1.arg2", 123);
  put(res, "group2.arg1", "bye");
  put(res, "group2.arg2", 2);
  CHECK_EQ(content(cfg), res);
}

// Checks whether both a synced variable and the corresponding entry in
// content(cfg) are equal to `value`.
#define CHECK_SYNCED(var, ...)                                                 \
  do {                                                                         \
    using ref_value_type = std::decay_t<decltype(var)>;                        \
    ref_value_type value{__VA_ARGS__};                                         \
    CHECK_EQ(var, value);                                                      \
    if (auto maybe_val = get_as<decltype(var)>(cfg, #var)) {                   \
      CHECK_EQ(*maybe_val, value);                                             \
    } else {                                                                   \
      auto cv = get_if(std::addressof(cfg.content), #var);                     \
      CAF_ERROR("expected type "                                               \
                << config_value::mapped_type_name<decltype(var)>()             \
                << ", got: " << cv->type_name());                              \
    }                                                                          \
  } while (false)

// Checks whether an entry in content(cfg) is equal to `value`.
#define CHECK_TEXT_ONLY(type, var, value)                                      \
  CHECK_EQ(get_as<type>(cfg, #var), value)

#define ADD(var) add(var, #var, "...")

#define VAR(type)                                                              \
  auto some_##type = type{};                                                   \
  options("global").add(some_##type, "some_" #type, "...")

#define NAMED_VAR(type, name)                                                  \
  auto name = type{};                                                          \
  options("global").add(name, #name, "...")

CAF_TEST(integers and integer containers options) {
  // Use a wild mess of "list-like" and "map-like" containers from the STL.
  using int_list = std::vector<int>;
  using int_list_list = std::list<std::deque<int>>;
  using int_map = std::unordered_map<std::string, int>;
  using int_list_map = std::map<std::string, std::unordered_set<int>>;
  using int_map_list = std::set<std::map<std::string, int>>;
  auto text = R"__(
    some_int = 42
    yet_another_int = 123
    some_int_list = [1, 2, 3]
    some_int_list_list = [[1, 2, 3], [4, 5, 6]]
    some_int_map = {a = 1, b = 2, c = 3}
    some_int_list_map = {a = [1, 2, 3], b = [4, 5, 6]}
    some_int_map_list = [{a = 1, b = 2, c = 3}, {d = 4, e = 5, f = 6}]
  )__";
  NAMED_VAR(int, some_other_int);
  VAR(int);
  VAR(int_list);
  VAR(int_list_list);
  VAR(int_map);
  VAR(int_list_map);
  VAR(int_map_list);
  parse(text, {"--some_other_int=23"});
  CHECK_SYNCED(some_int, 42);
  CHECK_SYNCED(some_other_int, 23);
  CHECK_TEXT_ONLY(int, yet_another_int, 123);
  CHECK_SYNCED(some_int_list, 1, 2, 3);
  CHECK_SYNCED(some_int_list_list, {1, 2, 3}, {4, 5, 6});
  CHECK_SYNCED(some_int_map, {{"a", 1}, {"b", 2}, {"c", 3}});
  CHECK_SYNCED(some_int_list_map, {{"a", {1, 2, 3}}, {"b", {4, 5, 6}}});
  CHECK_SYNCED(some_int_map_list, {{"a", 1}, {"b", 2}, {"c", 3}},
               {{"d", 4}, {"e", 5}, {"f", 6}});
}

CAF_TEST(basic and basic containers options) {
  using std::map;
  using std::string;
  using std::vector;
  using int_list = vector<int>;
  using bool_list = vector<bool>;
  using double_list = vector<double>;
  using timespan_list = vector<timespan>;
  using uri_list = vector<uri>;
  using string_list = vector<string>;
  using int_map = map<string, int>;
  using bool_map = map<string, bool>;
  using double_map = map<string, double>;
  using timespan_map = map<string, timespan>;
  using uri_map = map<string, uri>;
  using string_map = map<string, string>;
  auto text = R"__(
    some_int = 42
    some_bool = true
    some_double = 1e23
    some_timespan = 123ms
    some_uri = <foo:bar>
    some_string = "string"
    some_int_list = [1, 2, 3]
    some_bool_list = [false, true]
    some_double_list = [1., 2., 3.]
    some_timespan_list = [123ms, 234ms, 345ms]
    some_uri_list = [<foo:a>, <foo:b>, <foo:c>]
    some_string_list = ["a", "b", "c"]
    some_int_map = {a = 1, b = 2, c = 3}
    some_bool_map = {a = true, b = false}
    some_double_map = {a = 1., b = 2., c = 3.}
    some_timespan_map = {a = 123ms, b = 234ms, c = 345ms}
    some_uri_map = {a = <foo:a>, b = <foo:b>, c = <foo:c>}
    some_string_map = {a = "1", b = "2", c = "3"}
  )__";
  VAR(int);
  VAR(bool);
  VAR(double);
  VAR(timespan);
  VAR(uri);
  VAR(string);
  VAR(int_list);
  VAR(bool_list);
  VAR(double_list);
  VAR(timespan_list);
  VAR(uri_list);
  VAR(string_list);
  VAR(int_map);
  VAR(bool_map);
  VAR(double_map);
  VAR(timespan_map);
  VAR(uri_map);
  VAR(string_map);
  parse(text);
  MESSAGE("check primitive types");
  CHECK_SYNCED(some_int, 42);
  CHECK_SYNCED(some_bool, true);
  CHECK_SYNCED(some_double, 1e23);
  CHECK_SYNCED(some_timespan, 123_ms);
  CHECK_SYNCED(some_uri, "foo:bar"_u);
  CHECK_SYNCED(some_string, "string"s);
  MESSAGE("check list types");
  CHECK_SYNCED(some_int_list, 1, 2, 3);
  CHECK_SYNCED(some_bool_list, false, true);
  CHECK_SYNCED(some_double_list, 1., 2., 3.);
  CHECK_SYNCED(some_timespan_list, 123_ms, 234_ms, 345_ms);
  CHECK_SYNCED(some_uri_list, "foo:a"_u, "foo:b"_u, "foo:c"_u);
  CHECK_SYNCED(some_string_list, "a", "b", "c");
  MESSAGE("check dictionary types");
  CHECK_SYNCED(some_int_map, {"a", 1}, {"b", 2}, {"c", 3});
  CHECK_SYNCED(some_bool_map, {"a", true}, {"b", false});
  CHECK_SYNCED(some_double_map, {"a", 1.}, {"b", 2.}, {"c", 3.});
  CHECK_SYNCED(some_timespan_map, {"a", 123_ms}, {"b", 234_ms}, {"c", 345_ms});
  CHECK_SYNCED(some_uri_map, {"a", "foo:a"_u}, {"b", "foo:b"_u},
               {"c", "foo:c"_u});
  CHECK_SYNCED(some_string_map, {"a", "1"}, {"b", "2"}, {"c", "3"});
}

SCENARIO("config files allow both nested and dot-separated values") {
  GIVEN("the option my.answer.value") {
    config_option_adder{cfg.custom_options(), "my.answer"}
      .add<int32_t>("first", "the first answer")
      .add<int32_t>("second", "the second answer");
    std::vector<std::string> allowed_input_strings{
      "my { answer { first = 1, second = 2 } }",
      "my.answer { first = 1, second = 2 }",
      "my { answer.first = 1, answer.second = 2  }",
      "my.answer.first = 1, my.answer.second = 2",
      "my { answer { first = 1 }, answer.second = 2 }",
      "my { answer.first = 1, answer { second = 2} }",
      "my.answer.first = 1, my { answer { second = 2 } }",
    };
    auto make_result = [] {
      settings answer;
      answer["first"] = 1;
      answer["second"] = 2;
      settings my;
      my["answer"] = std::move(answer);
      settings result;
      result["my"] = std::move(my);
      return result;
    };
    auto result = make_result();
    for (const auto& input_string : allowed_input_strings) {
      WHEN("parsing the file input '" << input_string << "'") {
        std::istringstream input{input_string};
        auto err = cfg.parse(string_list{}, input);
        THEN("the actor system contains values for my.answer.(first|second)") {
          CHECK_EQ(err, error{});
          CHECK_EQ(get_or(cfg, "my.answer.first", -1), 1);
          CHECK_EQ(get_or(cfg, "my.answer.second", -1), 2);
          CHECK_EQ(content(cfg), result);
        }
      }
    }
  }
}

END_FIXTURE_SCOPE()
