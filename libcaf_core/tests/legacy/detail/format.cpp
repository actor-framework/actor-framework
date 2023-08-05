// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE detail.format

#include "caf/detail/format.hpp"

#include "core-test.hpp"

#if !defined(CAF_USE_STD_FORMAT) && !defined(CAF_USE_SYSTEM_LIBFMT)
#  define CAF_MINIMAL_FORMATTING
#endif

using namespace std::literals;

using caf::detail::format;
using caf::detail::format_to;

TEST_CASE("format strings without placeholders copies verbatim") {
  CHECK_EQ(format("hello world"), "hello world");
  CHECK_EQ(format("foo {{bar}}"), "foo {bar}");
  CHECK_EQ(format("foo {{bar}} baz"), "foo {bar} baz");
}

TEST_CASE("format strings without indexes iterate over their arguments") {
  CHECK_EQ(format("foo: {}{}", true, '!'), "foo: true!");
  CHECK_EQ(format("bar: {}{}", false, '?'), "bar: false?");
  CHECK_EQ(format("{} {} {} {} {}", 1, 2u, 2.5f, 4.5, "5"s), "1 2 2.5 4.5 5");
}

TEST_CASE("format strings with indexes uses the specified arguments") {
  CHECK_EQ(format("{1} {2} {0}", 3, 1, 2), "1 2 3");
  CHECK_EQ(format("{1} {0} {1}", 1, 2), "2 1 2");
}

TEST_CASE("format strings can specify rendering of floating point numbers") {
  CHECK_EQ(format("{}", 2.5), "2.5");
  CHECK_EQ(format("{:.3f}", 2.5), "2.500");
  CHECK_EQ(format("{:.3F}", 2.5), "2.500");
  CHECK_EQ(format("{:g}", 2.5), "2.5");
  CHECK_EQ(format("{:G}", 2.5), "2.5");
  CHECK_EQ(format("{:.0e}", 10.0), "1e+01");
  CHECK_EQ(format("{:.0E}", 10.0), "1E+01");
}

TEST_CASE("format strings can specify rendering of integers") {
  CHECK_EQ(format("{}", 42), "42");
  CHECK_EQ(format("{:d}", 42), "42");
  CHECK_EQ(format("{:c}", 42), "*");
  CHECK_EQ(format("{:o}", 42), "52");
  CHECK_EQ(format("{:#o}", 42), "052");
  CHECK_EQ(format("{:x}", 42), "2a");
  CHECK_EQ(format("{:X}", 42), "2A");
  CHECK_EQ(format("{:#x}", 42), "0x2a");
  CHECK_EQ(format("{:#X}", 42), "0X2A");
  CHECK_EQ(format("{}", 42u), "42");
  CHECK_EQ(format("{:d}", 42u), "42");
  CHECK_EQ(format("{:c}", 42u), "*");
  CHECK_EQ(format("{:o}", 42u), "52");
  CHECK_EQ(format("{:#o}", 42u), "052");
  CHECK_EQ(format("{:x}", 42u), "2a");
  CHECK_EQ(format("{:X}", 42u), "2A");
  CHECK_EQ(format("{:#x}", 42u), "0x2a");
  CHECK_EQ(format("{:#X}", 42u), "0X2A");
  CHECK_EQ(format("'{:+}' '{:-}' '{: }'", 1, 1, 1), "'+1' '1' ' 1'");
  CHECK_EQ(format("'{:+}' '{:-}' '{: }'", -1, -1, -1), "'-1' '-1' '-1'");
}

TEST_CASE("format strings may specify the width of the output") {
  CHECK_EQ(format("{0:0{1}}", 1, 2), "01");
  CHECK_EQ(format("{1:02} {0:02}", 1, 2), "02 01");
  CHECK_EQ(format("{:!<3}?{:!>3}", 0, 0), "0!!?!!0");
  CHECK_EQ(format("{:!^3}?{:!^3}", 'A', 'A'), "!A!?!A!");
  CHECK_EQ(format("{0:!^{1}}", 'A', 5), "!!A!!");
  CHECK_EQ(format("{:<3}?{:>3}", 0, 0), "0  ?  0");
}

TEST_CASE("format strings accept various string types as values") {
  using namespace std::literals;
  const auto* cstr = "C-string";
  CHECK_EQ(format("{}", cstr), "C-string");
  CHECK_EQ(format("{}", "string literal"), "string literal");
  CHECK_EQ(format("{}", "std::string"s), "std::string");
  CHECK_EQ(format("{}", "std::string_view"sv), "std::string_view");
}

TEST_CASE("format_to can incrementally build a string") {
  std::string str;
  format_to(std::back_inserter(str), "foo");
  CHECK_EQ(str, "foo");
  format_to(std::back_inserter(str), "bar");
  CHECK_EQ(str, "foobar");
  format_to(std::back_inserter(str), "baz");
  CHECK_EQ(str, "foobarbaz");
}

#if defined(CAF_ENABLE_EXCEPTIONS) && defined(CAF_MINIMAL_FORMATTING)

// Note: the standard version as well as libfmt (should) raise a compile-time
//       error for these test cases. Only our minimal implementation throws.

TEST_CASE("ill-formatted formatting strings throw") {
  CHECK_THROWS_AS(format("foo {"), std::logic_error);
  CHECK_THROWS_AS(format("foo } bar"), std::logic_error);
  CHECK_THROWS_AS(format("{1}", 1), std::logic_error);
}

#endif
