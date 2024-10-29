// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/json_reader.hpp"

#include "caf/test/approx.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/dictionary.hpp"
#include "caf/init_global_meta_objects.hpp"
#include "caf/log/test.hpp"

using namespace caf;

using namespace std::literals;

namespace {

struct circle;
struct dummy_struct;
struct dummy_user;
struct my_request;
struct phone_book;
struct point;
struct rectangle;
struct widget;

} // namespace

CAF_BEGIN_TYPE_ID_BLOCK(json_reader_test, caf::first_custom_type_id + 95)

  CAF_ADD_TYPE_ID(json_reader_test, (circle))
  CAF_ADD_TYPE_ID(json_reader_test, (dummy_struct))
  CAF_ADD_TYPE_ID(json_reader_test, (dummy_user))
  CAF_ADD_TYPE_ID(json_reader_test, (my_request))
  CAF_ADD_TYPE_ID(json_reader_test, (phone_book))
  CAF_ADD_TYPE_ID(json_reader_test, (point))
  CAF_ADD_TYPE_ID(json_reader_test, (rectangle))
  CAF_ADD_TYPE_ID(json_reader_test, (widget))

CAF_END_TYPE_ID_BLOCK(json_reader_test)

namespace {

struct my_request {
  int32_t a = 0;
  int32_t b = 0;
  my_request() = default;
  my_request(int a, int b) : a(a), b(b) {
    // nop
  }
};

inline bool operator==(const my_request& x, const my_request& y) {
  return std::tie(x.a, x.b) == std::tie(y.a, y.b);
}

template <class Inspector>
bool inspect(Inspector& f, my_request& x) {
  return f.object(x).fields(f.field("a", x.a), f.field("b", x.b));
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

struct phone_book {
  std::string city;
  std::map<std::string, int64_t> entries;
};

[[maybe_unused]] constexpr bool operator==(const phone_book& x,
                                           const phone_book& y) noexcept {
  return std::tie(x.city, x.entries) == std::tie(y.city, y.entries);
}

[[maybe_unused]] constexpr bool operator!=(const phone_book& x,
                                           const phone_book& y) noexcept {
  return !(x == y);
}

template <class Inspector>
bool inspect(Inspector& f, phone_book& x) {
  return f.object(x).fields(f.field("city", x.city),
                            f.field("entries", x.entries));
}

struct point {
  int32_t x;
  int32_t y;
};

template <class Inspector>
bool inspect(Inspector& f, point& x) {
  return f.object(x).fields(f.field("x", x.x), f.field("y", x.y));
}

[[maybe_unused]] constexpr bool operator==(point a, point b) noexcept {
  return a.x == b.x && a.y == b.y;
}

[[maybe_unused]] constexpr bool operator!=(point a, point b) noexcept {
  return !(a == b);
}

struct rectangle {
  point top_left;
  point bottom_right;
};

template <class Inspector>
bool inspect(Inspector& f, rectangle& x) {
  return f.object(x).fields(f.field("top-left", x.top_left),
                            f.field("bottom-right", x.bottom_right));
}

[[maybe_unused]] constexpr bool operator==(const rectangle& x,
                                           const rectangle& y) noexcept {
  return x.top_left == y.top_left && x.bottom_right == y.bottom_right;
}

[[maybe_unused]] constexpr bool operator!=(const rectangle& x,
                                           const rectangle& y) noexcept {
  return !(x == y);
}

struct dummy_user {
  std::string name;
  std::optional<std::string> nickname;
};

template <class Inspector>
bool inspect(Inspector& f, dummy_user& x) {
  return f.object(x).fields(f.field("name", x.name),
                            f.field("nickname", x.nickname));
}

struct circle {
  point center;
  int32_t radius;
};

template <class Inspector>
bool inspect(Inspector& f, circle& x) {
  return f.object(x).fields(f.field("center", x.center),
                            f.field("radius", x.radius));
}

[[maybe_unused]] constexpr bool operator==(const circle& x,
                                           const circle& y) noexcept {
  return x.center == y.center && x.radius == y.radius;
}

[[maybe_unused]] constexpr bool operator!=(const circle& x,
                                           const circle& y) noexcept {
  return !(x == y);
}

struct widget {
  std::string color;
  std::variant<rectangle, circle> shape;
};

template <class Inspector>
bool inspect(Inspector& f, widget& x) {
  return f.object(x).fields(f.field("color", x.color),
                            f.field("shape", x.shape));
}

[[maybe_unused]] inline bool operator==(const widget& x,
                                        const widget& y) noexcept {
  return x.color == y.color && x.shape == y.shape;
}

[[maybe_unused]] inline bool operator!=(const widget& x,
                                        const widget& y) noexcept {
  return !(x == y);
}

class widget_mapper : public type_id_mapper {
  std::string_view operator()(type_id_t type) const override {
    switch (type) {
      case type_id_v<rectangle>:
        return "rectangle";
      case type_id_v<circle>:
        return "circle";
      default:
        return query_type_name(type);
    }
  }
  type_id_t operator()(std::string_view name) const override {
    if (name == "rectangle")
      return type_id_v<rectangle>;
    else if (name == "circle")
      return type_id_v<circle>;
    else
      return query_type_id(name);
  }
};

struct fixture {
  // Adds a test case for a given input and expected output.
  template <class T>
  void add_test_case(std::string_view input, T val) {
    auto f = [this, input, obj{std::move(val)}]() -> bool {
      auto& this_test = test::runnable::current();
      auto tmp = T{};
      auto res = this_test.check(reader.load(input))    // parse JSON
                 && this_test.check(reader.apply(tmp)); // deserialize object
      if (res) {
        if constexpr (std::is_same_v<T, message>)
          res = this_test.check_eq(to_string(tmp), to_string(obj));
        else if constexpr (std::is_arithmetic_v<T>)
          res = this_test.check_eq(tmp, test::approx{obj});
        else
          res = this_test.check_eq(tmp, obj);
      }
      if (!res)
        log::test::debug("rejected input: {}", input);
      return res;
    };
    test_cases.emplace_back(std::move(f));
  }

  // Specialization for `widget` to add a test case for a given input.
  void add_test_case(std::string_view input, widget val) {
    auto f = [this, input, obj{std::move(val)}]() -> bool {
      widget_mapper mapper_instance;
      reader.mapper(&mapper_instance);
      auto& this_test = test::runnable::current();
      auto tmp = widget{};
      auto res = this_test.check(reader.load(input))    // parse JSON
                 && this_test.check(reader.apply(tmp)); // deserialize object
      if (res) {
        res = this_test.check_eq(tmp, obj);
      }
      if (!res)
        log::test::debug("rejected input: {}", input);
      return res;
    };
    test_cases.emplace_back(std::move(f));
  }

  // Specialization for `std::string` so we can test all `read_json_string`
  // overloads.
  void add_test_case(std::string_view input, std::string val) {
    auto f = [this, input, obj{std::move(val)}]() -> bool {
      auto& this_test = test::runnable::current();
      auto tmp = std::string{};
      // Test overload for reading from memory.
      auto res = this_test.check(reader.load(input))    // parse JSON
                 && this_test.check(reader.apply(tmp)); // deserialize object
      if (res) {
        res = this_test.check_eq(tmp, obj);
      } else {
        log::test::error("failed to parse JSON: {}", input);
      }
      if (!res) {
        return false;
      }
      // Test overload for reading from an STL stream.
      auto tmp2 = std::string{};
      std::istringstream istream{std::string{input}};
      res = this_test.check(reader.load_from(istream)) // parse JSON
            && this_test.check(reader.apply(tmp2));    // deserialize object
      if (res) {
        res = this_test.check_eq(tmp2, obj);
      } else {
        log::test::error("failed to parse JSON: {}", input);
      }
      return res;
    };
    test_cases.emplace_back(std::move(f));
  }

  // Adds a test case that should fail.
  template <class T>
  void add_neg_test_case(std::string_view input) {
    auto f = [this, input]() -> bool {
      auto tmp = T{};
      auto res = reader.load(input)    // parse JSON
                 && reader.apply(tmp); // deserialize object
      test::runnable::current().check(!res);
      if (res) {
        log::test::error("got unexpected output: {}", tmp);
        return false;
      }
      if constexpr (std::is_same_v<T, std::string>) {
        // Also test parsing from STL stream.
        std::istringstream istream{std::string{input}};
        res = reader.load(input)    // parse JSON
              && reader.apply(tmp); // deserialize object
        test::runnable::current().check(!res);
        if (res) {
          log::test::error("got unexpected output: {}", tmp);
          return false;
        }
      }
      return true;
    };
    test_cases.emplace_back(std::move(f));
  }

  template <class T, class... Ts>
  std::vector<T> ls(Ts... xs) {
    std::vector<T> result;
    (result.emplace_back(std::move(xs)), ...);
    return result;
  }

  template <class T, class... Ts>
  std::set<T> set(Ts... xs) {
    std::set<T> result;
    (result.emplace(std::move(xs)), ...);
    return result;
  }

  template <class T>
  using dict = dictionary<T>;

  fixture();

  json_reader reader;

  std::vector<std::function<bool()>> test_cases;
};

fixture::fixture() {
  using i32_list = std::vector<int32_t>;
  using str_list = std::vector<std::string>;
  using str_set = std::set<std::string>;
  add_test_case(R"_(true)_", true);
  add_test_case(R"_(false)_", false);
  add_test_case(R"_([true, false])_", ls<bool>(true, false));
  add_test_case(R"_([1, 2, 3])_", ls<int32_t>(1, 2, 3));
  add_test_case(R"_([[1, 2], [3], []])_",
                ls<i32_list>(ls<int32_t>(1, 2), ls<int32_t>(3), ls<int32_t>()));
  add_test_case(R"_(2.0)_", 2.0);
  add_test_case(R"_([2.0, 4.0, 8.0])_", ls<double>(2.0, 4.0, 8.0));
  add_test_case(R"_("hello \"world\"!")_", std::string{R"_(hello "world"!)_"});
  add_test_case(R"_(["hello", "world"])_", ls<std::string>("hello", "world"));
  add_test_case(R"_(["hello", "world"])_", set<std::string>("hello", "world"));
  add_test_case(R"_({"a": 1, "b": 2})_", my_request(1, 2));
  add_test_case(R"_({"a": 1, "b": 2})_", dict<int>({{"a", 1}, {"b", 2}}));
  add_test_case(R"_({"xs": ["x1", "x2"], "ys": ["y1", "y2"]})_",
                dict<str_list>({{"xs", ls<std::string>("x1", "x2")},
                                {"ys", ls<std::string>("y1", "y2")}}));
  add_test_case(R"_({"xs": ["x1", "x2"], "ys": ["y1", "y2"]})_",
                dict<str_set>({{"xs", set<std::string>("x1", "x2")},
                               {"ys", set<std::string>("y1", "y2")}}));
  add_test_case(R"_([{"@type": "my_request", "a": 1, "b": 2}])_",
                make_message(my_request(1, 2)));
  add_test_case(
    R"_({"top-left":{"x":100,"y":200},"bottom-right":{"x":10,"y":20}})_",
    rectangle{{100, 200}, {10, 20}});
  add_test_case(R"({
                     "@type": "phone_book",
                      "city": "Model City",
                      "entries": {
                        "Bob": 5556837,
                        "Jon": 5559347
                      }
                   })",
                phone_book{"Model City", {{"Bob", 5556837}, {"Jon", 5559347}}});
  add_test_case(R"({
                     "@type": "widget",
                     "color": "red",
                     "@shape-type": "circle",
                     "shape": {
                       "center": {"x": 15, "y": 15},
                       "radius": 5
                     }
                   })",
                widget{"red", circle{{15, 15}, 5}});
  add_test_case(R"({
                     "@type": "widget",
                     "color": "blue",
                     "@shape-type": "rectangle",
                     "shape": {
                       "top-left": {"x": 10, "y": 10},
                       "bottom-right": {"x": 20, "y": 20}
                     }
                   })",
                widget{"blue", rectangle{{10, 10}, {20, 20}}});
  // Test cases for integers that are in bound.
  add_test_case(R"_(-128)_", int8_t{INT8_MIN});
  add_test_case(R"_(127)_", int8_t{INT8_MAX});
  add_test_case(R"_(-32768)_", int16_t{INT16_MIN});
  add_test_case(R"_(32767)_", int16_t{INT16_MAX});
  add_test_case(R"_(-2147483648)_", int32_t{INT32_MIN});
  add_test_case(R"_(2147483647)_", int32_t{INT32_MAX});
  add_test_case(R"_(-9223372036854775808)_", int64_t{INT64_MIN});
  add_test_case(R"_(9223372036854775807)_", int64_t{INT64_MAX});
  // Test cases for unsigned integers that are in bound.
  add_test_case(R"_(0)_", uint8_t{0});
  add_test_case(R"_(255)_", uint8_t{UINT8_MAX});
  add_test_case(R"_(0)_", uint16_t{0});
  add_test_case(R"_(65535)_", uint16_t{UINT16_MAX});
  add_test_case(R"_(0)_", uint32_t{0});
  add_test_case(R"_(4294967295)_", uint32_t{UINT32_MAX});
  add_test_case(R"_(0)_", uint64_t{0});
  add_test_case(R"_(18446744073709551615)_", uint64_t{UINT64_MAX});
  // Test cases for integers that are out of bound.
  add_neg_test_case<int8_t>(R"_(-129)_");
  add_neg_test_case<int8_t>(R"_(128)_");
  add_neg_test_case<int16_t>(R"_(-32769)_");
  add_neg_test_case<int16_t>(R"_(32768)_");
  add_neg_test_case<int32_t>(R"_(-2147483649)_");
  add_neg_test_case<int32_t>(R"_(2147483648)_");
  add_neg_test_case<int64_t>(R"_(-9223372036854775809)_");
  add_neg_test_case<int64_t>(R"_(9223372036854775808)_");
  // Test cases for unsigned integers that are out of bound.
  add_neg_test_case<uint8_t>(R"_(-1)_");
  add_neg_test_case<uint8_t>(R"_(256)_");
  add_neg_test_case<uint16_t>(R"_(-1)_");
  add_neg_test_case<uint16_t>(R"_(65536)_");
  add_neg_test_case<uint32_t>(R"_(-1)_");
  add_neg_test_case<uint32_t>(R"_(4294967296)_");
  add_neg_test_case<uint64_t>(R"_(-1)_");
  add_neg_test_case<uint64_t>(R"_(18446744073709551616)_");
  // Test cases for proper handling of whitespace.
  add_test_case("[1, \r\n2,\f\t\v3]", ls<int32_t>(1, 2, 3));
  add_test_case("\r\n{\r\n\"a\":\r\n1, \"b\"\r\n:\r\n2}\r\n", my_request(1, 2));
  // Test cases for proper handling of code point escape sequences.
  // Single byte utf-8 sequences.
  add_test_case(
    R"_("\u0048\u0065\u006c\u006c\u006f\u002c\u0020\u0022\u0057\u006f\u0072\u006c\u0064\u0022\u0021")_",
    std::string{R"_(Hello, "World"!)_"});
  // Escape sequence test.
  add_test_case(R"_("\u0008")_", std::string{"\b"});
  add_test_case(R"_("\u005c")_", std::string{R"_(\)_"});
  add_test_case(R"_("\u005c\u005c")_", std::string{R"_(\\)_"});
  // Two byte utf-8 sequences.
  add_test_case(R"_("\u0107\u010d\u017e\u0161\u0111")_", std::string{"ćčžšđ"});
  // Three byte utf-8 sequences.
  add_test_case(R"_("\u20AC\u2192\u221E")_", std::string{"€→∞"});
  // Four byte utf-8 sequences.
  add_test_case(R"_("\ud834\udd1e")_", std::string{"𝄞"});
  // Failing tests
  add_neg_test_case<std::string>(R"_("\ud900")_");
  add_neg_test_case<std::string>(R"_("\u06c")_");
}

} // namespace

WITH_FIXTURE(fixture) {

TEST("json baselines") {
  size_t baseline_index = 0;
  detail::monotonic_buffer_resource resource;
  for (auto& f : test_cases) {
    log::test::debug("test case at index {}", baseline_index++);
    if (!f())
      if (auto reason = reader.get_error())
        log::test::debug("JSON reader stopped due to: {}", reason);
  }
}

SCENARIO("mappers enable custom type names in JSON input") {
  GIVEN("a custom mapper") {
    class custom_mapper : public type_id_mapper {
      std::string_view operator()(type_id_t type) const override {
        switch (type) {
          case type_id_v<std::string>:
            return "String";
          case type_id_v<int32_t>:
            return "Int";
          default:
            return query_type_name(type);
        }
      }
      type_id_t operator()(std::string_view name) const override {
        if (name == "String")
          return type_id_v<std::string>;
        else if (name == "Int")
          return type_id_v<int32_t>;
        else
          return query_type_id(name);
      }
    };
    custom_mapper mapper_instance;
    WHEN("reading a variant from JSON") {
      using value_type = std::variant<int32_t, std::string>;
      THEN("the custom mapper translates between external and internal names") {
        json_reader reader;
        reader.mapper(&mapper_instance);
        auto value = value_type{};
        auto input1 = R"_({"@value-type": "String", "value": "hello world"})_"s;
        if (check(reader.load(input1))) {
          if (!check(reader.apply(value)))
            log::test::debug("reader reported error: {}", reader.get_error());
          if (check(std::holds_alternative<std::string>(value)))
            check_eq(std::get<std::string>(value), "hello world"s);
        } else {
          log::test::debug("reader reported error: {}", reader.get_error());
        }
        reader.reset();
        auto input2 = R"_({"@value-type": "Int", "value": 42})_"sv;
        if (check(reader.load(input2))) {
          if (!check(reader.apply(value)))
            log::test::debug("reader reported error: {}", reader.get_error());
          if (check(std::holds_alternative<int32_t>(value)))
            check_eq(std::get<int32_t>(value), 42);
        } else {
          log::test::debug("reader reported error: {}", reader.get_error());
        }
      }
    }
  }
}

} // WITH_FIXTURE(fixture)

TEST_INIT() {
  caf::init_global_meta_objects<caf::id_block::json_reader_test>();
}
