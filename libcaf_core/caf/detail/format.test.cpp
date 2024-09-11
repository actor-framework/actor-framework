// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/format.hpp"

#include "caf/test/test.hpp"

#include <vector>

#if !defined(CAF_USE_STD_FORMAT) && !defined(CAF_USE_SYSTEM_LIBFMT)
#  define CAF_MINIMAL_FORMATTING
#endif

using namespace caf;
using namespace std::literals;

#ifdef CAF_USE_STD_FORMAT

using caf::detail::fmt_fwd;
// Types that have a formatter overload are forwarded as is.
static_assert(std::is_same_v<decltype(fmt_fwd(std::declval<char>())), char&&>);
static_assert(std::is_same_v<decltype(fmt_fwd(std::declval<int>())), int&&>);
static_assert(
  std::is_same_v<decltype(fmt_fwd(std::declval<const char&>())), const char&>);
static_assert(
  std::is_same_v<decltype(fmt_fwd(std::declval<const int&>())), const int&>);
// Types without a formatter overload are forwarded as std::string.
static_assert(
  std::is_same_v<decltype(fmt_fwd(std::declval<std::vector<int>>())),
                 std::string>);

#endif

TEST("format strings without placeholders copies verbatim") {
  check_eq(detail::format("hello world"), "hello world");
  check_eq(detail::format("foo {{bar}}"), "foo {bar}");
  check_eq(detail::format("foo {{bar}} baz"), "foo {bar} baz");
}

TEST("format strings without indexes iterate over their arguments") {
  check_eq(detail::format("foo: {}{}", true, '!'), "foo: true!");
  check_eq(detail::format("bar: {}{}", false, '?'), "bar: false?");
  check_eq(detail::format("{} {} {} {} {}", 1, 2u, 2.5f, 4.5, "5"s),
           "1 2 2.5 4.5 5");
}

TEST("format strings with indexes uses the specified arguments") {
  check_eq(detail::format("{1} {2} {0}", 3, 1, 2), "1 2 3");
  check_eq(detail::format("{1} {0} {1}", 1, 2), "2 1 2");
}

TEST("format strings can specify rendering of floating point numbers") {
  check_eq(detail::format("{}", 2.5), "2.5");
  check_eq(detail::format("{:.3f}", 2.5), "2.500");
  check_eq(detail::format("{:.3F}", 2.5), "2.500");
  check_eq(detail::format("{:g}", 2.5), "2.5");
  check_eq(detail::format("{:G}", 2.5), "2.5");
  check_eq(detail::format("{:.0e}", 10.0), "1e+01");
  check_eq(detail::format("{:.0E}", 10.0), "1E+01");
}

TEST("format strings can specify rendering of integers") {
  check_eq(detail::format("{}", 42), "42");
  check_eq(detail::format("{:d}", 42), "42");
  check_eq(detail::format("{:c}", 42), "*");
  check_eq(detail::format("{:o}", 42), "52");
  check_eq(detail::format("{:#o}", 42), "052");
  check_eq(detail::format("{:x}", 42), "2a");
  check_eq(detail::format("{:X}", 42), "2A");
  check_eq(detail::format("{:#x}", 42), "0x2a");
  check_eq(detail::format("{:#X}", 42), "0X2A");
  check_eq(detail::format("{}", 42u), "42");
  check_eq(detail::format("{:d}", 42u), "42");
  check_eq(detail::format("{:c}", 42u), "*");
  check_eq(detail::format("{:o}", 42u), "52");
  check_eq(detail::format("{:#o}", 42u), "052");
  check_eq(detail::format("{:x}", 42u), "2a");
  check_eq(detail::format("{:X}", 42u), "2A");
  check_eq(detail::format("{:#x}", 42u), "0x2a");
  check_eq(detail::format("{:#X}", 42u), "0X2A");
  check_eq(detail::format("{:+} '{:-}' '{: }'", 1, 1, 1), "+1 '1' ' 1'");
  check_eq(detail::format("{:+} '{:-}' '{: }'", -1, -1, -1), "-1 '-1' '-1'");
}

TEST("format strings may specify the width of the output") {
  check_eq(detail::format("{0:0{1}}", 1, 2), "01");
  check_eq(detail::format("{1:02} {0:02}", 1, 2), "02 01");
  check_eq(detail::format("{:!<3}?{:!>3}", 0, 0), "0!!?!!0");
  check_eq(detail::format("{:!^3}?{:!^3}", 'A', 'A'), "!A!?!A!");
  check_eq(detail::format("{0:!^{1}}", 'A', 5), "!!A!!");
  check_eq(detail::format("{:<3}?{:>3}", 0, 0), "0  ?  0");
}

TEST("format strings accept various string types as values") {
  using namespace std::literals;
  const auto* cstr = "C-string";
  check_eq(detail::format("{}", cstr), "C-string");
  check_eq(detail::format("{}", "string literal"), "string literal");
  check_eq(detail::format("{}", "std::string"s), "std::string");
  check_eq(detail::format("{}", "std::string_view"sv), "std::string_view");
}

TEST("format_to can incrementally build a string") {
  std::string str;
  detail::format_to(std::back_inserter(str), "foo");
  check_eq(str, "foo");
  detail::format_to(std::back_inserter(str), "bar");
  check_eq(str, "foobar");
  detail::format_to(std::back_inserter(str), "baz");
  check_eq(str, "foobarbaz");
}

#if defined(CAF_ENABLE_EXCEPTIONS) && defined(CAF_MINIMAL_FORMATTING)

// Note: the standard version as well as libfmt (should) raise a compile-time
//       error for these test cases. Only our minimal implementation throws.

TEST("ill-formatted formatting strings throw") {
  check_throws<std::logic_error>([] { detail::format("foo {"); });
  check_throws<std::logic_error>([] { detail::format("foo } bar"); });
  check_throws<std::logic_error>([] { detail::format("{1}", 1); });
}

#endif
