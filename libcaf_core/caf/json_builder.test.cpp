// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/json_builder.hpp"

#include "caf/test/test.hpp"

#include "caf/init_global_meta_objects.hpp"
#include "caf/json_value.hpp"
#include "caf/log/test.hpp"

#include <string_view>

using namespace caf;
using namespace std::literals;

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
  check(builder.value(int32_t{42}));
  auto val = builder.seal();
  check(val.is_integer());
  check_eq(val.to_integer(), 42);
}

TEST("floating point") {
  check(builder.value(4.2));
  auto val = builder.seal();
  check(val.is_double());
  check_eq(val.to_double(), 4.2);
}

TEST("boolean") {
  check(builder.value(true));
  auto val = builder.seal();
  check(val.is_bool());
  check_eq(val.to_bool(), true);
}

TEST("string") {
  check(builder.value("Hello, world!"sv));
  auto val = builder.seal();
  check(val.is_string());
  check_eq(val.to_string(), "Hello, world!"sv);
}

TEST("array") {
  auto xs = std::vector{1, 2, 3};
  check(builder.apply(xs));
  auto val = builder.seal();
  check(val.is_array());
  check_eq(printed(val), "[1, 2, 3]"sv);
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
