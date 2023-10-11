// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/parser/read_number.hpp"

#include "caf/test/caf_test_main.hpp"
#include "caf/test/test.hpp"

#include "caf/detail/nearly_equal.hpp"
#include "caf/detail/parser/add_ascii.hpp"
#include "caf/detail/parser/sub_ascii.hpp"
#include "caf/expected.hpp"
#include "caf/parser_state.hpp"
#include "caf/pec.hpp"

#include <string>
#include <string_view>
#include <variant>

using namespace caf;

struct number_consumer {
  std::variant<int64_t, double> x;

  void value(double y) {
    x = y;
  }

  void value(int64_t y) {
    x = y;
  }

  void value(uint64_t y) {
    if (y < INT64_MAX) {
      value(static_cast<int64_t>(y));
    } else {
      auto& this_test = test::runnable::current();
      this_test.fail("number consumer called with an uint64_t that is larger "
                     "than INT64_MAX");
    }
  }
};

struct range_consumer {
  std::vector<int64_t> xs;

  void value(double) {
    auto& this_test = test::runnable::current();
    this_test.fail("range consumer called with a double");
  }

  void value(int64_t y) {
    xs.emplace_back(y);
  }

  void value(uint64_t y) {
    if (y < INT64_MAX) {
      value(static_cast<int64_t>(y));
    } else {
      auto& this_test = test::runnable::current();
      this_test.fail("range consumer called with an uint64_t that is larger "
                     "than INT64_MAX");
    }
  }
};

struct res_t {
  std::variant<pec, double, int64_t> val;
};

std::string to_string(const res_t& x) {
  return std::visit([](auto& y) { return deep_to_string(y); }, x.val);
}

bool operator==(const res_t& x, const res_t& y) {
  if (x.val.index() != y.val.index())
    return false;
  if (std::holds_alternative<pec>(x.val))
    return std::get<pec>(x.val) == std::get<pec>(y.val);
  if (std::holds_alternative<double>(x.val))
    return detail::nearly_equal(std::get<double>(x.val),
                                std::get<double>(y.val));
  return std::get<int64_t>(x.val) == std::get<int64_t>(y.val);
}

struct numbers_parser {
  res_t operator()(std::string_view str) {
    number_consumer f;
    string_parser_state res{str.begin(), str.end()};
    detail::parser::read_number(res, f);
    if (res.code == pec::success)
      return std::visit([](auto val) { return res_t{val}; }, f.x);
    else
      return res_t{res.code};
  }
};

struct range_parser {
  expected<std::vector<int64_t>> operator()(std::string_view str) {
    range_consumer f;
    string_parser_state res{str.begin(), str.end()};
    detail::parser::read_number(res, f, std::true_type{}, std::true_type{});
    if (res.code == pec::success)
      return expected<std::vector<int64_t>>{std::move(f.xs)};
    else
      return make_error(res);
  }
};

template <class T>
std::enable_if_t<std::is_integral_v<T>, res_t> res(T x) {
  return res_t{static_cast<int64_t>(x)};
}

template <class T>
std::enable_if_t<std::is_floating_point_v<T>, res_t> res(T x) {
  return res_t{static_cast<double>(x)};
}

struct fixture {
  numbers_parser p;
  range_parser r;
};

WITH_FIXTURE(fixture) {

TEST("add ascii - unsigned") {
  using detail::parser::add_ascii;
  auto rd = [](std::string_view str) -> expected<uint8_t> {
    uint8_t x = 0;
    for (auto c : str)
      if (!add_ascii<10>(x, c))
        return pec::integer_overflow;
    return x;
  };
  for (int i = 0; i < 256; ++i)
    check_eq(rd(std::to_string(i)), static_cast<uint8_t>(i));
  for (int i = 256; i < 513; ++i)
    check_eq(rd(std::to_string(i)), pec::integer_overflow);
}

TEST("add ascii - signed") {
  auto rd = [](std::string_view str) -> expected<int8_t> {
    int8_t x = 0;
    for (auto c : str)
      if (!detail::parser::add_ascii<10>(x, c))
        return pec::integer_overflow;
    return x;
  };
  for (int i = 0; i < 128; ++i)
    check_eq(rd(std::to_string(i)), static_cast<int8_t>(i));
  for (int i = 128; i < 513; ++i)
    check_eq(rd(std::to_string(i)), pec::integer_overflow);
}

TEST("sub ascii") {
  auto rd = [](std::string_view str) -> expected<int8_t> {
    int8_t x = 0;
    for (auto c : str)
      if (!detail::parser::sub_ascii<10>(x, c))
        return pec::integer_underflow;
    return x;
  };
  // Using sub_ascii in this way behaves as if we'd prefix the number with a
  // minus sign, i.e., "123" will result in -123.
  for (int i = 1; i < 129; ++i)
    check_eq(rd(std::to_string(i)), static_cast<int8_t>(-i));
  for (int i = 129; i < 513; ++i)
    check_eq(rd(std::to_string(i)), pec::integer_underflow);
}

#define CHECK_NUMBER(x) check_eq(p(#x), res(x))

TEST("binary numbers") {
  CHECK_NUMBER(0b0);
  CHECK_NUMBER(0b10);
  CHECK_NUMBER(0b101);
  CHECK_NUMBER(0B1001);
  CHECK_NUMBER(-0b0);
  CHECK_NUMBER(-0b101);
  CHECK_NUMBER(-0B1001);
}

TEST("octal numbers") {
  // valid numbers
  CHECK_NUMBER(00);
  CHECK_NUMBER(010);
  CHECK_NUMBER(0123);
  CHECK_NUMBER(0777);
  CHECK_NUMBER(-00);
  CHECK_NUMBER(-0123);
  // invalid numbers
  check_eq(p("018"), res_t{pec::trailing_character});
}

TEST("decimal numbers") {
  CHECK_NUMBER(0);
  CHECK_NUMBER(10);
  CHECK_NUMBER(123);
  CHECK_NUMBER(-0);
  CHECK_NUMBER(-123);
}

TEST("hexadecimal numbers") {
  // valid numbers
  CHECK_NUMBER(0x0);
  CHECK_NUMBER(0x10);
  CHECK_NUMBER(0X123);
  CHECK_NUMBER(0xAF01);
  CHECK_NUMBER(-0x0);
  CHECK_NUMBER(-0x123);
  CHECK_NUMBER(-0xaf01);
  // invalid numbers
  check_eq(p("0xFG"), res_t{pec::trailing_character});
  check_eq(p("0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"),
           res_t{pec::integer_overflow});
  check_eq(p("-0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"),
           res_t{pec::integer_underflow});
}

TEST("floating point numbers") {
  CHECK_NUMBER(0.0);
  CHECK_NUMBER(.0);
  CHECK_NUMBER(0.);
  CHECK_NUMBER(1.1);
  CHECK_NUMBER(.1);
  CHECK_NUMBER(1.);
  CHECK_NUMBER(0.123);
  CHECK_NUMBER(.123);
  CHECK_NUMBER(123.456);
  CHECK_NUMBER(-0.0);
  CHECK_NUMBER(-.0);
  CHECK_NUMBER(-0.);
  CHECK_NUMBER(-1.1);
  CHECK_NUMBER(-.1);
  CHECK_NUMBER(-1.);
  CHECK_NUMBER(-0.123);
  CHECK_NUMBER(-.123);
  CHECK_NUMBER(-123.456);
}

TEST("integer mantissa with positive exponent") {
  CHECK_NUMBER(321E1);
  CHECK_NUMBER(321e1);
  CHECK_NUMBER(321e+1);
  CHECK_NUMBER(123e2);
  CHECK_NUMBER(-4e2);
  CHECK_NUMBER(1e1);
  CHECK_NUMBER(1e2);
  CHECK_NUMBER(1e3);
  CHECK_NUMBER(1e4);
  CHECK_NUMBER(1e5);
  CHECK_NUMBER(1e6);
}

TEST("integer mantissa with negative exponent") {
  // valid numbers
  CHECK_NUMBER(321E-1);
  CHECK_NUMBER(321e-1);
  CHECK_NUMBER(123e-2);
  CHECK_NUMBER(-4e-2);
  CHECK_NUMBER(1e-1);
  CHECK_NUMBER(1e-2);
  CHECK_NUMBER(1e-3);
  CHECK_NUMBER(1e-4);
  CHECK_NUMBER(1e-5);
  CHECK_NUMBER(1e-6);
  // invalid numbers
  check_eq(p("-9.9999e-e511"), res_t{pec::unexpected_character});
  check_eq(p("-9.9999e-511"), res_t{pec::exponent_underflow});
}

TEST("fractional mantissa with positive exponent") {
  CHECK_NUMBER(3.21E1);
  CHECK_NUMBER(3.21e+1);
  CHECK_NUMBER(3.21e+1);
  CHECK_NUMBER(12.3e2);
  CHECK_NUMBER(-0.001e3);
  CHECK_NUMBER(0.0001e5);
  CHECK_NUMBER(-42.001e3);
  CHECK_NUMBER(42.0001e5);
}

TEST("fractional mantissa with negative exponent") {
  CHECK_NUMBER(3.21E-1);
  CHECK_NUMBER(3.21e-1);
  CHECK_NUMBER(12.3e-2);
  CHECK_NUMBER(-0.001e-3);
  CHECK_NUMBER(-0.0001e-5);
  CHECK_NUMBER(-42.001e-3);
  CHECK_NUMBER(-42001e-6);
  CHECK_NUMBER(-42.0001e-5);
}

#define CHECK_RANGE(expr, ...)                                                 \
  check_eq(r(expr), std::vector<int64_t>({__VA_ARGS__}))

TEST("a range from n to n is just n") {
  CHECK_RANGE("0..0", 0);
  CHECK_RANGE("1..1", 1);
  CHECK_RANGE("2..2", 2);
  CHECK_RANGE("101..101", 101);
  CHECK_RANGE("101..101..1", 101);
  CHECK_RANGE("101..101..2", 101);
  CHECK_RANGE("101..101..-1", 101);
  CHECK_RANGE("101..101..-2", 101);
}

TEST("ranges are either ascending or descending") {
  CHECK_RANGE("0..1", 0, 1);
  CHECK_RANGE("0..2", 0, 1, 2);
  CHECK_RANGE("0..3", 0, 1, 2, 3);
  CHECK_RANGE("3..0", 3, 2, 1, 0);
  CHECK_RANGE("3..1", 3, 2, 1);
  CHECK_RANGE("3..2", 3, 2);
}

TEST("ranges can use positive step values") {
  CHECK_RANGE("2..6..2", 2, 4, 6);
  CHECK_RANGE("3..8..3", 3, 6);
}

TEST("ranges can use negative step values") {
  CHECK_RANGE("6..2..-2", 6, 4, 2);
  CHECK_RANGE("8..3..-3", 8, 5);
}

TEST("ranges can use signed integers") {
  CHECK_RANGE("+2..+6..+2", 2, 4, 6);
  CHECK_RANGE("+6..+2..-2", 6, 4, 2);
  CHECK_RANGE("+2..-2..-2", 2, 0, -2);
  CHECK_RANGE("-2..+2..+2", -2, 0, 2);
}

#define CHECK_ERR(expr, enum_value)                                            \
  if (auto res = r(expr)) {                                                    \
    fail("expected expression to produce to an error");                        \
  } else {                                                                     \
    check_eq(res.error(), enum_value);                                         \
  }

TEST("the parser rejects invalid step values") {
  CHECK_ERR("-2..+2..-2", pec::invalid_range_expression);
  CHECK_ERR("+2..-2..+2", pec::invalid_range_expression);
  CHECK_ERR("+2..-2..0", pec::invalid_range_expression);
}

} // WITH_FIXTURE(fixture)

CAF_TEST_MAIN()
