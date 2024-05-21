// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/json_builder.hpp"

#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/init_global_meta_objects.hpp"
#include "caf/json_value.hpp"
#include "caf/log/test.hpp"
#include "caf/span.hpp"

#include <string_view>

using namespace caf;
using namespace std::literals;

namespace caf {

template <class T>
bool operator<(const span<T>& lhs, const span<T>& rhs) {
  return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(),
                                      rhs.end());
}

} // namespace caf

namespace {

struct my_request;
struct rectangle;
struct point;

} // namespace

CAF_BEGIN_TYPE_ID_BLOCK(json_builder_test, caf::first_custom_type_id + 85)

  CAF_ADD_TYPE_ID(json_builder_test, (my_request))
  CAF_ADD_TYPE_ID(json_builder_test, (rectangle))
  CAF_ADD_TYPE_ID(json_builder_test, (point))

CAF_END_TYPE_ID_BLOCK(json_builder_test)

namespace {

struct my_request {
  int32_t a = 0;
  int32_t b = 0;
  my_request() = default;
  my_request(int a, int b) : a(a), b(b) {
    // nop
  }
};

template <class Inspector>
bool inspect(Inspector& f, my_request& x) {
  return f.object(x).fields(f.field("a", x.a), f.field("b", x.b));
}

struct point {
  int32_t x;
  int32_t y;
};

template <class Inspector>
bool inspect(Inspector& f, point& x) {
  return f.object(x).fields(f.field("x", x.x), f.field("y", x.y));
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

struct fixture {
  fixture() {
    builder.skip_object_type_annotation(true);
  }

  std::string printed(const json_value& val, size_t indentation_factor = 0) {
    std::string result;
    val.print_to(result, indentation_factor);
    return result;
  }

  json_builder builder;
};

} // namespace

namespace {

WITH_FIXTURE(fixture) {

TEST("empty JSON value") {
  auto val = builder.seal();
  check(val.is_null());
}

TEST("integer") {
  SECTION("int8") {
    check(builder.value(int8_t{42}));
  }
  SECTION("uint8") {
    check(builder.value(uint8_t{42}));
  }
  SECTION("int16") {
    check(builder.value(int16_t{42}));
  }
  SECTION("uint16") {
    check(builder.value(uint16_t{42}));
  }
  SECTION("int32") {
    check(builder.value(int32_t{42}));
  }
  SECTION("uint32") {
    check(builder.value(uint32_t{42}));
  }
  SECTION("int64") {
    check(builder.value(int64_t{42}));
  }
  SECTION("uint64") {
    check(builder.value(uint64_t{42}));
  }
  auto val = builder.seal();
  check(val.is_integer());
  check_eq(val.to_integer(), 42);
}

TEST("float") {
  SECTION("value") {
    check(builder.value(4.2f));
    auto val = builder.seal();
    check(val.is_double());
    check_eq(val.to_double(), 4.2f);
  }
  SECTION("array") {
    auto xs = std::vector{4.2f, 4.2f, 4.2f};
    check(builder.apply(xs));
    auto val = builder.seal();
    check(val.is_array());
    check_eq(printed(val), "[4.2, 4.2, 4.2]"sv);
  }
}

TEST("double") {
  check(builder.value(4.2));
  auto val = builder.seal();
  check(val.is_double());
  check_eq(val.to_double(), 4.2);
}

TEST("long double") {
  check(builder.value(4.2l));
  auto val = builder.seal();
  check(val.is_double());
  check_eq(val.to_double(), 4.2);
}

TEST("byte") {
  check(builder.value(std::byte{42}));
  auto val = builder.seal();
  check(val.is_integer());
  check_eq(val.to_integer(), 42);
}

TEST("boolean") {
  SECTION("value") {
    check(builder.value(true));
    auto val = builder.seal();
    check(val.is_bool());
    check_eq(val.to_bool(), true);
  }
  SECTION("array") {
    auto xs = std::vector{true, false, true};
    check(builder.apply(xs));
    auto val = builder.seal();
    check(val.is_array());
    check_eq(printed(val), "[true, false, true]"sv);
  }
  SECTION("error") {
    check(builder.apply(int64_t{42}));
    auto val = builder.seal();
    check(!builder.value(true));
  }
}

TEST("string") {
  SECTION("value") {
    check(builder.value("Hello, world!"sv));
    auto val = builder.seal();
    check(val.is_string());
    check_eq(val.to_string(), "Hello, world!"sv);
  }
  SECTION("array") {
    auto xs = std::vector{"Hello"sv, "world!"sv};
    check(builder.apply(xs));
    auto val = builder.seal();
    check(val.is_array());
    check_eq(printed(val), R"_(["Hello", "world!"])_"sv);
  }
  SECTION("error") {
    check(builder.apply(int64_t{42}));
    auto val = builder.seal();
    check(!builder.value("Hello, world!"sv));
  }
}

TEST("byte span") {
  auto bytes = std::vector<std::byte>{std::byte{'A'}, std::byte{'B'},
                                      std::byte{'C'}};
  SECTION("value") {
    check(builder.value(make_span(bytes)));
    auto val = builder.seal();
    check(val.is_string());
    check_eq(val.to_string(), "414243"sv);
  }
  SECTION("array") {
    auto xs = std::vector{make_span(bytes), make_span(bytes)};
    check(builder.apply(xs));
    auto val = builder.seal();
    check(val.is_array());
    check_eq(printed(val), R"_(["414243", "414243"])_"sv);
  }
  SECTION("error") {
    check(builder.apply(int64_t{42}));
    auto val = builder.seal();
    check(!builder.value(make_span(bytes)));
  }
}

TEST("u16string") {
  check(!builder.value(u"Hello, world!"s));
}

TEST("u32string") {
  check(!builder.value(U"Hello, world!"s));
}

TEST("array") {
  auto xs = std::vector{1, 2, 3};
  check(builder.apply(xs));
  auto val = builder.seal();
  check(val.is_array());
  check_eq(printed(val), "[1, 2, 3]"sv);
}

TEST("reset") {
  SECTION("sealed value") {
    check(builder.value(42));
    builder.seal();
    builder.reset();
  }
  SECTION("unsealed value") {
    check(builder.value(42));
    builder.reset();
  }
}

TEST("fail") {
  check(builder.apply(int64_t{42}));
  auto val = builder.seal();
  check(!builder.value(int32_t{64}));
}

TEST("flat object") {
  auto req = my_request{10, 20};
  if (!check(builder.apply(req))) {
    log::test::debug("builder: {}", builder.get_error());
  }
  auto val = builder.seal();
  check(val.is_object());
  check_eq(printed(val), R"_({"a": 10, "b": 20})_");
}

TEST("flat object with type annotation") {
  builder.skip_object_type_annotation(false);
  auto req = my_request{10, 20};
  if (!check(builder.apply(req))) {
    log::test::debug("builder: {}", builder.get_error());
  }
  auto val = builder.seal();
  check(val.is_object());
  check_eq(printed(val), R"_({"@type": "my_request", "a": 10, "b": 20})_");
}

TEST("begin field") {
  check(builder.begin_object(1, "circle"));
  SECTION("is present") {
    SECTION("missing index") {
      auto circle_type = std::vector<uint16_t>{295};
      check(!builder.begin_field("foo", true, make_span(circle_type), 1));
    }
    SECTION("missing query type") {
      auto circle_type = std::vector<uint16_t>{1000};
      check(!builder.begin_field("foo", true, make_span(circle_type), 0));
    }
    SECTION("present query type") {
      auto circle_type = std::vector<uint16_t>{295};
      check(builder.begin_field("foo", true, make_span(circle_type), 0));
      check(builder.value(42));
      check(builder.end_field());
      check(builder.end_object());
      auto val = builder.seal();
      check(val.is_object());
      check_eq(printed(val), R"_({"foo": 42, "@foo-type": "circle"})_");
    }
  }
  SECTION("is not present") {
    auto circle_type = std::vector<uint16_t>{295};
    SECTION("don't skip empty fields") {
      builder.skip_empty_fields(false);
      check(builder.begin_field("foo", false, make_span(circle_type), 0));
      check(!builder.value(42));
      check(builder.end_field());
      check(builder.end_object());
      auto val = builder.seal();
      check(val.is_object());
      check_eq(printed(val), R"_({"foo": null})_");
    }
    SECTION("skip empty fields") {
      check(builder.begin_field("foo", false, make_span(circle_type), 0));
      check(!builder.value(42));
      check(builder.end_field());
      check(builder.end_object());
      auto val = builder.seal();
      check(val.is_object());
      check_eq(printed(val), R"_({})_");
    }
  }
}

TEST("begin associative array") {
  SECTION("valid associative array") {
    auto circle_type = std::vector<uint16_t>{295};
    check(builder.begin_tuple(1));
    check(builder.begin_associative_array(1));
    builder.skip_empty_fields(false);
    check(builder.begin_field("foo", false, make_span(circle_type), 0));
    auto val = builder.seal();
    check(val.is_array());
    check_eq(printed(val), R"_([{"foo": null}])_");
  }
  SECTION("invalid associative array") {
    check(builder.value(42));
    check(!builder.begin_associative_array(0));
    check(!builder.end_tuple());
  }
}

TEST("begin sequence") {
  SECTION("valid array") {
    builder.skip_empty_fields(false);
    auto circle_type = std::vector<uint16_t>{295};
    check(builder.begin_sequence(1));
    check(builder.begin_sequence(1));
    check(!builder.begin_field("foo", false, make_span(circle_type), 0));
    check(builder.end_sequence());
    check(builder.end_sequence());
    auto val = builder.seal();
    check(val.is_array());
    check_eq(printed(val), R"_([[]])_");
  }
  SECTION("invalid array") {
    check(builder.value(42));
    check(!builder.begin_sequence(1));
    check(!builder.end_tuple());
  }
}

TEST("unexpected object") {
  auto circle_type = std::vector<uint16_t>{295};
  check(!builder.begin_field("foo", false, make_span(circle_type), 0));
}

SCENARIO("json_builder can build json for maps with keys") {
  GIVEN("a map with string as keys") {
    std::map<std::string, int> map{{"one", 1}};
    WHEN("building the map") {
      THEN("the map is built correctly") {
        check(builder.apply(map));
        auto val = builder.seal();
        check(val.is_object());
        check_eq(printed(val), R"_({"one": 1})_");
      }
    }
  }
  GIVEN("a map with number as keys") {
    std::map<int, int> map{{1, 1}};
    WHEN("building the map") {
      THEN("the map is built correctly") {
        check(builder.apply(map));
        auto val = builder.seal();
        check(val.is_object());
        check_eq(printed(val), R"_({"1": 1})_");
      }
    }
  }
  GIVEN("a map with boolean as keys") {
    std::map<bool, int> boolean_map{{true, 1}};
    WHEN("building the map") {
      THEN("the map is built correctly") {
        check(builder.apply(boolean_map));
        auto val = builder.seal();
        check(val.is_object());
        check_eq(printed(val), R"_({"true": 1})_");
      }
    }
  }
  GIVEN("a map with byte span as keys") {
    auto bytes = std::vector<std::byte>{std::byte{'A'}, std::byte{'B'}};
    std::map<span<const std::byte>, int> span_map{{make_span(bytes), 1}};
    WHEN("building the map") {
      THEN("the map is built correctly") {
        check(builder.apply(span_map));
        auto val = builder.seal();
        check(val.is_object());
        check_eq(printed(val), R"_({"4142": 1})_");
      }
    }
  }
}

namespace {

constexpr std::string_view rect_str = R"_({
  "top-left": {
    "x": 10,
    "y": 10
  },
  "bottom-right": {
    "x": 20,
    "y": 20
  }
})_";

} // namespace

TEST("nested object") {
  auto rect = rectangle{{10, 10}, {20, 20}};
  if (!check(builder.apply(rect))) {
    log::test::debug("builder: {}", builder.get_error());
  }
  auto val = builder.seal();
  check(val.is_object());
  check_eq(printed(val, 2), rect_str);
}

namespace {

constexpr std::string_view annotated_rect_str = R"_({
  "@type": "rectangle",
  "top-left": {
    "x": 10,
    "y": 10
  },
  "bottom-right": {
    "x": 20,
    "y": 20
  }
})_";

} // namespace

TEST("nested object with type annotation") {
  builder.skip_object_type_annotation(false);
  auto rect = rectangle{{10, 10}, {20, 20}};
  if (!check(builder.apply(rect))) {
    log::test::debug("builder: {}", builder.get_error());
  }
  auto val = builder.seal();
  check(val.is_object());
  check_eq(printed(val, 2), annotated_rect_str);
}

} // WITH_FIXTURE(fixture)

TEST_INIT() {
  caf::init_global_meta_objects<caf::id_block::json_builder_test>();
}

} // namespace
