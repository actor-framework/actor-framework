// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/stringification_inspector.hpp"

#include "caf/test/approx.hpp"
#include "caf/test/test.hpp"

#include "caf/type_id.hpp"

using namespace caf;
using namespace caf::detail;
using namespace std::literals;

namespace {

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

} // namespace

TEST("stringification of numbers") {
  int value = 42;
  check_eq(stringification_inspector::render(value), "42"s);
  check_eq(stringification_inspector::render(&value), "*42"s);
  check_eq(stringification_inspector::render(-42), "-42"s);
  check_eq(stringification_inspector::render(INT32_MAX), "2147483647"s);
  check_eq(stringification_inspector::render(INT32_MIN), "-2147483648"s);
  check_eq(stringification_inspector::render(UINT64_MAX),
           "18446744073709551615"s);
  check_eq(stringification_inspector::render(INT64_MAX),
           "9223372036854775807"s);
  check_eq(stringification_inspector::render(INT64_MIN),
           "-9223372036854775808"s);
  check_eq(stringification_inspector::render(3.14), "3.14"s);
  check_eq(stringification_inspector::render(-3.14), "-3.14"s);
  check_eq(stringification_inspector::render(3.141592f), "3.141592"s);
  check_eq(stringification_inspector::render(-3.141592f), "-3.141592"s);
  check_eq(stringification_inspector::render(3.14159265358979323846L),
           "3.141593"s);
  check_eq(stringification_inspector::render(-3.14159265358979323846L),
           "-3.141593"s);
}

TEST("stringification of booleans") {
  check_eq(stringification_inspector::render(true), "true"s);
  check_eq(stringification_inspector::render(false), "false"s);
  check_eq(stringification_inspector::render(std::vector<bool>{true, false}),
           "[true, false]"s);
}

TEST("stringification of std::string") {
  check_eq(stringification_inspector::render(""s), R"()"s);
  check_eq(stringification_inspector::render(R"(hello)"s), R"(hello)"s);
  check_eq(stringification_inspector::render(R"(hello "world")"s),
           R"(hello "world")"s);
  check_eq(stringification_inspector::render(R"(hello\world)"s),
           R"(hello\world)"s);
}

TEST("stringification of objects with type_name") {
  std::string result;
  caf::detail::stringification_inspector inspector{result};
  check_eq(inspector.begin_object(0, "std::string"), true);
  check_eq(inspector.value("Hello World"s), true);
  check_eq(inspector.end_object(), true);
  check_eq(result, R"("Hello World")"s);
}

TEST("stringification of std::vector") {
  check_eq(caf::detail::stringification_inspector::render(
             std::vector<int>{1, 2, 3}),
           "[1, 2, 3]"s);
  check_eq(stringification_inspector::render(
             std::vector<std::string>{"hello", "world"}),
           R"(["hello", "world"])"s);
  check_eq(stringification_inspector::render(
             std::vector<std::vector<int>>{{1, 2}, {3, 4}}),
           "[[1, 2], [3, 4]]"s);
  check_eq(stringification_inspector::render(
             std::vector<std::vector<std::string>>{{"hello", "world"}}),
           R"([["hello", "world"]])"s);
}

TEST("stringification of std::chrono::duration") {
  check_eq(stringification_inspector::render(42s), "42s"s);
  check_eq(stringification_inspector::render(42ms), "42ms"s);
  check_eq(stringification_inspector::render(42us), "42us"s);
  check_eq(stringification_inspector::render(42ns), "42ns"s);
  check_eq(stringification_inspector::render(
             std::chrono::duration<float>{3.14}),
           "3.14s"s);
  check_eq(stringification_inspector::render(
             std::chrono::duration<double>{3.14}),
           "3.14s"s);
  check_eq(stringification_inspector::render(timespan{42}), "42ns"s);
  check_eq(stringification_inspector::render(timespan{42 * 1000}), "42us"s);
  check_eq(stringification_inspector::render(timespan{42 * 1000 * 1000}),
           "42ms"s);
}

TEST("stringification of byte span") {
  check_eq(stringification_inspector::render(std::byte{42}), "2A"s);
  auto hello = "hello"sv;
  check_eq(stringification_inspector::render(as_bytes(make_span(hello))),
           "68656C6C6F"s);
}

TEST("stringification of custom types") {
  check_eq(stringification_inspector::render(dummy_user{"John"}),
           "anonymous(\"John\", null, null)"s);
  check_eq(stringification_inspector::render(dummy_user{"John", "Doe"}),
           "anonymous(\"John\", *\"Doe\", null)"s);
  check_eq(stringification_inspector::render(dummy_user{"John", "Doe", "JD"}),
           "anonymous(\"John\", *\"Doe\", *\"JD\")"s);
}

TEST("stringification of std::map") {
  check_eq(stringification_inspector::render(
             std::map<int, int>{{1, 2}, {3, 4}}),
           "{1 = 2, 3 = 4}"s);
  check_eq(stringification_inspector::render(
             std::map<std::string, int>{{"hello", 42}}),
           "{\"hello\" = 42}"s);
  check_eq(stringification_inspector::render(
             std::map<int, std::string>{{42, "hello"}}),
           "{42 = \"hello\"}"s);
  check_eq(stringification_inspector::render(
             std::map<std::string, std::string>{{"hello", "world"}}),
           "{\"hello\" = \"world\"}"s);
}

TEST("stringification of pointers") {
  int value = 42;
  check_eq(stringification_inspector::render(&value), "*42"s);
  void* ptr = nullptr;
  check_eq(stringification_inspector::render(ptr), "null"s);
}
