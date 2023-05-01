// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE json_builder

#include "caf/json_builder.hpp"

#include "core-test.hpp"

#include "caf/json_value.hpp"

#include <string_view>

using namespace caf;
using namespace std::literals;

namespace {

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

BEGIN_FIXTURE_SCOPE(fixture)

TEST_CASE("empty JSON value") {
  auto val = builder.seal();
  CHECK(val.is_null());
}

TEST_CASE("integer") {
  CHECK(builder.value(int32_t{42}));
  auto val = builder.seal();
  CHECK(val.is_integer());
  CHECK_EQ(val.to_integer(), 42);
}

TEST_CASE("floating point") {
  CHECK(builder.value(4.2));
  auto val = builder.seal();
  CHECK(val.is_double());
  CHECK_EQ(val.to_double(), 4.2);
}

TEST_CASE("boolean") {
  CHECK(builder.value(true));
  auto val = builder.seal();
  CHECK(val.is_bool());
  CHECK_EQ(val.to_bool(), true);
}

TEST_CASE("string") {
  CHECK(builder.value("Hello, world!"sv));
  auto val = builder.seal();
  CHECK(val.is_string());
  CHECK_EQ(val.to_string(), "Hello, world!"sv);
}

TEST_CASE("array") {
  auto xs = std::vector{1, 2, 3};
  CHECK(builder.apply(xs));
  auto val = builder.seal();
  CHECK(val.is_array());
  CHECK_EQ(printed(val), "[1, 2, 3]"sv);
}

TEST_CASE("flat object") {
  auto req = my_request{10, 20};
  if (!CHECK(builder.apply(req))) {
    MESSAGE("builder: " << builder.get_error());
  }
  auto val = builder.seal();
  CHECK(val.is_object());
  CHECK_EQ(printed(val), R"_({"a": 10, "b": 20})_");
}

TEST_CASE("flat object with type annotation") {
  builder.skip_object_type_annotation(false);
  auto req = my_request{10, 20};
  if (!CHECK(builder.apply(req))) {
    MESSAGE("builder: " << builder.get_error());
  }
  auto val = builder.seal();
  CHECK(val.is_object());
  CHECK_EQ(printed(val), R"_({"@type": "my_request", "a": 10, "b": 20})_");
}

namespace {

constexpr string_view rect_str = R"_({
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

TEST_CASE("nested object") {
  auto rect = rectangle{{10, 10}, {20, 20}};
  if (!CHECK(builder.apply(rect))) {
    MESSAGE("builder: " << builder.get_error());
  }
  auto val = builder.seal();
  CHECK(val.is_object());
  CHECK_EQ(printed(val, 2), rect_str);
}

namespace {

constexpr string_view annotated_rect_str = R"_({
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

TEST_CASE("nested object with type annotation") {
  builder.skip_object_type_annotation(false);
  auto rect = rectangle{{10, 10}, {20, 20}};
  if (!CHECK(builder.apply(rect))) {
    MESSAGE("builder: " << builder.get_error());
  }
  auto val = builder.seal();
  CHECK(val.is_object());
  CHECK_EQ(printed(val, 2), annotated_rect_str);
}

END_FIXTURE_SCOPE()
