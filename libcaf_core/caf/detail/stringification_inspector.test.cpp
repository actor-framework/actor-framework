// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/stringification_inspector.hpp"

#include "caf/test/test.hpp"

#include "caf/expected.hpp"
#include "caf/init_global_meta_objects.hpp"
#include "caf/sec.hpp"
#include "caf/term.hpp"
#include "caf/timestamp.hpp"
#include "caf/type_id.hpp"

#include <array>
#include <tuple>

using namespace caf;
using namespace std::literals;

using caf::detail::stringification_inspector;

namespace {

// -- custom types for testing ------------------------------------------------

struct dummy_user {
  std::string first_name;
  std::optional<std::string> last_name;
  std::optional<std::string> nickname;
};

template <class Inspector>
bool inspect(Inspector& f, dummy_user& x) {
  return f.object(x).fields(f.field("first_name", x.first_name),
                            f.field("last_name", x.last_name),
                            f.field("nickname", x.nickname));
}

struct point2d {
  int32_t x;
  int32_t y;
};

template <class Inspector>
bool inspect(Inspector& f, point2d& p) {
  return f.object(p).fields(f.field("x", p.x), f.field("y", p.y));
}

struct rectangle {
  point2d top_left;
  point2d bottom_right;
};

template <class Inspector>
bool inspect(Inspector& f, rectangle& r) {
  return f.object(r).fields(f.field("top_left", r.top_left),
                            f.field("bottom_right", r.bottom_right));
}

struct mixed_bag {
  int32_t count;
  double ratio;
  std::string label;
  std::vector<int> values;
};

template <class Inspector>
bool inspect(Inspector& f, mixed_bag& m) {
  return f.object(m).fields(f.field("count", m.count),
                            f.field("ratio", m.ratio),
                            f.field("label", m.label),
                            f.field("values", m.values));
}

struct colored_point {
  point2d position;
  term color;
};

template <class Inspector>
bool inspect(Inspector& f, colored_point& cp) {
  return f.object(cp).fields(f.field("position", cp.position),
                             f.field("color", cp.color));
}

// -- helper function ---------------------------------------------------------

template <class... Args>
auto do_render(const Args&... args) {
  std::string result;
  stringification_inspector f{result};
  auto ok = (f.apply(detail::as_mutable_ref(args)) && ...);
  if (!ok) {
    return to_string(f.get_error());
  }
  return result;
}

} // namespace

CAF_BEGIN_TYPE_ID_BLOCK(stringifcation_test, first_custom_type_id + 140)

  CAF_ADD_TYPE_ID(stringifcation_test, (point2d))
  CAF_ADD_TYPE_ID(stringifcation_test, (rectangle))
  CAF_ADD_TYPE_ID(stringifcation_test, (mixed_bag))
  CAF_ADD_TYPE_ID(stringifcation_test, (colored_point))
  CAF_ADD_TYPE_ID(stringifcation_test, (dummy_user))

CAF_END_TYPE_ID_BLOCK(stringifcation_test)

TEST_INIT() {
  caf::init_global_meta_objects<caf::id_block::stringifcation_test>();
}

TEST("stringification of numbers") {
  int value = 42;
  check_eq(do_render(value), "42"s);
  check_eq(do_render(&value), "*42"s);
  check_eq(do_render(-42), "-42"s);
  check_eq(do_render(INT32_MAX), "2147483647"s);
  check_eq(do_render(INT32_MIN), "-2147483648"s);
  check_eq(do_render(UINT64_MAX), "18446744073709551615"s);
  check_eq(do_render(INT64_MAX), "9223372036854775807"s);
  check_eq(do_render(INT64_MIN), "-9223372036854775808"s);
  check_eq(do_render(3.14), "3.14"s);
  check_eq(do_render(-3.14), "-3.14"s);
  check_eq(do_render(3.141592f), "3.141592"s);
  check_eq(do_render(-3.141592f), "-3.141592"s);
  check_eq(do_render(3.14159265358979323846L), "3.141593"s);
  check_eq(do_render(-3.14159265358979323846L), "-3.141593"s);
}

TEST("stringification of booleans") {
  check_eq(do_render(true), "true"s);
  check_eq(do_render(false), "false"s);
  check_eq(do_render(std::vector<bool>{true, false}), "[true, false]"s);
}

TEST("stringification of std::string") {
  check_eq(do_render(""s), R"__("")__");
  check_eq(do_render(R"(hello)"s), R"__("hello")__");
  check_eq(do_render(R"(hello "world")"s), R"__("hello \"world\"")__");
  check_eq(do_render(R"(hello\world)"s), R"__("hello\\world")__");
}

TEST("stringification of objects with type_name") {
  std::string result;
  stringification_inspector inspector{result};
  check_eq(inspector.begin_object(0, "std::string"), true);
  check_eq(inspector.value("Hello World"s), true);
  check_eq(inspector.end_object(), true);
  check_eq(result, R"("Hello World")"s);
}

TEST("stringification of std::vector") {
  check_eq(do_render(std::vector<int>{1, 2, 3}), "[1, 2, 3]"s);
  check_eq(do_render(std::vector<std::string>{"hello", "world"}),
           R"(["hello", "world"])"s);
  check_eq(do_render(std::vector<std::vector<int>>{{1, 2}, {3, 4}}),
           "[[1, 2], [3, 4]]"s);
  check_eq(do_render(std::vector<std::vector<std::string>>{{"hello", "world"}}),
           R"([["hello", "world"]])"s);
}

TEST("stringification of std::chrono::duration") {
  check_eq(do_render(42s), "42s"s);
  check_eq(do_render(42ms), "42ms"s);
  check_eq(do_render(42us), "42us"s);
  check_eq(do_render(42ns), "42ns"s);
  check_eq(do_render(std::chrono::duration<float>{3.14}), "3.14s"s);
  check_eq(do_render(std::chrono::duration<double>{3.14}), "3.14s"s);
  check_eq(do_render(timespan{42}), "42ns"s);
  check_eq(do_render(timespan{42 * 1000}), "42us"s);
  check_eq(do_render(timespan{42 * 1000 * 1000}), "42ms"s);
}

TEST("stringification of byte span") {
  check_eq(do_render(std::byte{42}), "2A"s);
  auto hello = "hello"sv;
  check_eq(do_render(as_bytes(std::span{hello})), "68656C6C6F"s);
}

TEST("stringification of custom types") {
  using std::nullopt;
  check_eq(do_render(dummy_user{"John"s, nullopt, nullopt}),
           R"_(dummy_user("John", null, null))_"s);
  check_eq(do_render(dummy_user{"John"s, "Doe"s, nullopt}),
           R"_(dummy_user("John", *"Doe", null))_"s);
  check_eq(do_render(dummy_user{"John"s, "Doe"s, "JD"s}),
           R"_(dummy_user("John", *"Doe", *"JD"))_"s);
}

TEST("stringification of std::map") {
  check_eq(do_render(std::map<int, int>{{1, 2}, {3, 4}}), "{1 = 2, 3 = 4}"s);
  check_eq(do_render(std::map<std::string, int>{{"hello", 42}}),
           "{\"hello\" = 42}"s);
  check_eq(do_render(std::map<int, std::string>{{42, "hello"}}),
           "{42 = \"hello\"}"s);
  check_eq(do_render(std::map<std::string, std::string>{{"hello", "world"}}),
           "{\"hello\" = \"world\"}"s);
}

TEST("stringification of pointers") {
  int value = 42;
  check_eq(do_render(&value), "*42"s);
  void* ptr = nullptr;
  check_eq(do_render(ptr), "null"s);
}

TEST("stringification of various integer types") {
  // Signed integers
  check_eq(do_render(int8_t{-128}), "-128"s);
  check_eq(do_render(int8_t{127}), "127"s);
  check_eq(do_render(int16_t{-32768}), "-32768"s);
  check_eq(do_render(int16_t{32767}), "32767"s);
  check_eq(do_render(int32_t{-2147483648}), "-2147483648"s);
  check_eq(do_render(int32_t{2147483647}), "2147483647"s);
  check_eq(do_render(int64_t{-9223372036854775807LL - 1}),
           "-9223372036854775808"s);
  check_eq(do_render(int64_t{9223372036854775807LL}), "9223372036854775807"s);
  // Unsigned integers
  check_eq(do_render(uint8_t{0}), "0"s);
  check_eq(do_render(uint8_t{255}), "255"s);
  check_eq(do_render(uint16_t{0}), "0"s);
  check_eq(do_render(uint16_t{65535}), "65535"s);
  check_eq(do_render(uint32_t{0}), "0"s);
  check_eq(do_render(uint32_t{4294967295}), "4294967295"s);
  check_eq(do_render(uint64_t{0}), "0"s);
  check_eq(do_render(uint64_t{18446744073709551615ULL}),
           "18446744073709551615"s);
}

TEST("stringification of timestamps") {
  // ISO 8601 format regex: YYYY-MM-DDTHH:MM:SS.mmm (with optional timezone)
  auto iso8601
    = R"(^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(\.\d+)?([+-]\d{2}:\d{2}|Z)?$)";
  check_matches(do_render(make_timestamp()), iso8601);
}

TEST("stringification of std::optional") {
  check_eq(do_render(std::optional<int>{}), "null"s);
  check_eq(do_render(std::optional<int>{42}), "*42"s);
  check_eq(do_render(std::optional<std::string>{}), "null"s);
  check_eq(do_render(std::optional<std::string>{"hello"}), "*\"hello\""s);
  check_eq(do_render(std::optional<double>{3.14}), "*3.14"s);
}

TEST("stringification of std::pair") {
  check_eq(do_render(std::pair{1, 2}), "[1, 2]"s);
  check_eq(do_render(std::pair{"hello"s, 42}), "[\"hello\", 42]"s);
  check_eq(do_render(std::pair{3.14, true}), "[3.14, true]"s);
}

TEST("stringification of std::tuple") {
  check_eq(do_render(std::tuple{1}), "[1]"s);
  check_eq(do_render(std::tuple{1, 2, 3}), "[1, 2, 3]"s);
  check_eq(do_render(std::tuple{"hello"s, 42, true}), "[\"hello\", 42, true]"s);
  check_eq(do_render(std::tuple{1.5, "test"s, false, 99}),
           "[1.5, \"test\", false, 99]"s);
}

TEST("stringification of std::array") {
  check_eq(do_render(std::array<int, 3>{1, 2, 3}), "[1, 2, 3]"s);
  check_eq(do_render(std::array<std::string, 2>{"hello", "world"}),
           "[\"hello\", \"world\"]"s);
  check_eq(do_render(std::array<bool, 4>{true, false, true, false}),
           "[true, false, true, false]"s);
}

TEST("stringification of empty containers") {
  check_eq(do_render(std::vector<int>{}), "[]"s);
  check_eq(do_render(std::map<int, int>{}), "{}"s);
  check_eq(do_render(std::array<int, 0>{}), "[]"s);
}

TEST("stringification of enums with to_string") {
  check_eq(do_render(term::red), "red"s);
  check_eq(do_render(term::green), "green"s);
  check_eq(do_render(term::blue), "blue"s);
  check_eq(do_render(std::vector<term>{term::red, term::green, term::blue}),
           "[red, green, blue]"s);
}

TEST("stringification of nested structs") {
  auto pt = point2d{10, 20};
  check_eq(do_render(pt), "point2d(10, 20)"s);
  auto rect = rectangle{{0, 0}, {100, 200}};
  check_eq(do_render(rect), "rectangle(point2d(0, 0), point2d(100, 200))"s);
  auto cp = colored_point{{5, 10}, term::blue};
  check_eq(do_render(cp), "colored_point(point2d(5, 10), blue)"s);
}

TEST("stringification of structs with mixed types") {
  auto bag = mixed_bag{42, 3.14, "test", {1, 2, 3}};
  check_eq(do_render(bag), "mixed_bag(42, 3.14, \"test\", [1, 2, 3])"s);
}

TEST("stringification of vectors containing custom types") {
  std::vector<point2d> points{{1, 2}, {3, 4}, {5, 6}};
  check_eq(do_render(points), "[point2d(1, 2), point2d(3, 4), point2d(5, 6)]"s);
}

TEST("stringification of maps with custom value types") {
  std::map<std::string, point2d> named_points{{"origin", {0, 0}},
                                              {"target", {10, 20}}};
  check_eq(do_render(named_points),
           "{\"origin\" = point2d(0, 0), \"target\" = point2d(10, 20)}"s);
}

TEST("stringification of deeply nested structures") {
  std::vector<std::vector<std::vector<int>>> cube{{{1, 2}, {3, 4}},
                                                  {{5, 6}, {7, 8}}};
  check_eq(do_render(cube), "[[[1, 2], [3, 4]], [[5, 6], [7, 8]]]"s);
}

TEST("stringification of C-style strings") {
  const char* cstr = "hello";
  check_eq(do_render(cstr), "\"hello\""s);
  const char* null_cstr = nullptr;
  check_eq(do_render(null_cstr), "null"s);
}

TEST("stringification of expected with value") {
  // expected<int> with value
  check_eq(do_render(expected<int>{42}), "42"s);
  // expected<string> with value
  check_eq(do_render(expected<std::string>{"hello"s}), "\"hello\""s);
  // expected<double> with value
  check_eq(do_render(expected<double>{3.14}), "3.14"s);
  // expected<bool> with value
  check_eq(do_render(expected<bool>{true}), "true"s);
}

TEST("stringification of expected with error") {
  // expected<int> with error
  auto err_int = expected<int>{make_error(sec::runtime_error)};
  check_eq(do_render(err_int), "!runtime_error"s);
  // expected<string> with error
  auto err_str = expected<std::string>{make_error(sec::invalid_argument)};
  check_eq(do_render(err_str), "!invalid_argument"s);
  // expected with error message
  auto err_msg
    = expected<int>{make_error(sec::runtime_error, "something failed")};
  check_eq(do_render(err_msg), "!runtime_error(\"something failed\")"s);
}

TEST("stringification of expected<void>") {
  // expected<void> with value (success)
  check_eq(do_render(expected<void>{}), "unit"s);
  // expected<void> with error
  auto err_void = expected<void>{make_error(sec::runtime_error)};
  check_eq(do_render(err_void), "!runtime_error"s);
}

TEST("stringification of nested expected") {
  // Vector of expected values
  std::vector<expected<int>> vec{expected<int>{1}, expected<int>{2},
                                 expected<int>{make_error(sec::runtime_error)}};
  check_eq(do_render(vec), "[1, 2, !runtime_error]"s);
}

TEST("stringification of expected with custom type") {
  // expected<point2d> with value
  auto pt = point2d{10, 20};
  check_eq(do_render(expected<point2d>{pt}), "point2d(10, 20)"s);
  // expected<point2d> with error
  auto err_pt = expected<point2d>{make_error(sec::invalid_argument)};
  check_eq(do_render(err_pt), "!invalid_argument"s);
}
