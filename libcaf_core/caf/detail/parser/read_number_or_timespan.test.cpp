// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/parser/read_number_or_timespan.hpp"

#include "caf/test/caf_test_main.hpp"
#include "caf/test/test.hpp"

#include "caf/detail/nearly_equal.hpp"
#include "caf/parser_state.hpp"

#include <string>
#include <string_view>
#include <variant>

using namespace caf;
using namespace std::chrono;

struct number_or_timespan_parser_consumer {
  std::variant<int64_t, double, timespan> x;
  template <class T>
  void value(T y) {
    x = y;
  }
};

struct res_t {
  std::variant<pec, double, int64_t, timespan> val;
};

std::string to_string(const res_t& x) {
  return deep_to_string(x.val);
}

bool operator==(const res_t& x, const res_t& y) {
  if (x.val.index() != y.val.index())
    return false;
  if (std::holds_alternative<pec>(x.val))
    return std::get<pec>(x.val) == std::get<pec>(y.val);
  if (std::holds_alternative<double>(x.val))
    return detail::nearly_equal(std::get<double>(x.val),
                                std::get<double>(y.val));
  if (std::holds_alternative<int64_t>(x.val))
    return std::get<int64_t>(x.val) == std::get<int64_t>(y.val);
  return std::get<timespan>(x.val) == std::get<timespan>(y.val);
}

struct number_or_timespan_parser {
  res_t operator()(std::string_view str) {
    number_or_timespan_parser_consumer f;
    string_parser_state res{str.begin(), str.end()};
    detail::parser::read_number_or_timespan(res, f);
    if (res.code == pec::success)
      return std::visit([](auto val) { return res_t{val}; }, f.x);
    else
      return res_t{res.code};
  }
};

struct fixture {
  number_or_timespan_parser p;
};

template <class T>
std::enable_if_t<std::is_integral_v<T>, res_t> res(T x) {
  return res_t{static_cast<int64_t>(x)};
}

template <class T>
std::enable_if_t<std::is_floating_point_v<T>, res_t> res(T x) {
  return res_t{static_cast<double>(x)};
}

template <class Rep, class Period>
res_t res(std::chrono::duration<Rep, Period> x) {
  return res_t{std::chrono::duration_cast<timespan>(x)};
}

WITH_FIXTURE(fixture) {

TEST("valid numbers and timespans") {
  check_eq(p("123"), res(123));
  check_eq(p("123.456"), res(123.456));
  check_eq(p("123s"), res(seconds(123)));
  check_eq(p("123ns"), res(nanoseconds(123)));
  check_eq(p("123ms"), res(milliseconds(123)));
  check_eq(p("123us"), res(microseconds(123)));
  check_eq(p("123min"), res(minutes(123)));
}

TEST("invalid timespans") {
  check_eq(p("12.3s"), res_t{pec::fractional_timespan});
  check_eq(p("12.3n"), res_t{pec::fractional_timespan});
  check_eq(p("12.3ns"), res_t{pec::fractional_timespan});
  check_eq(p("12.3m"), res_t{pec::fractional_timespan});
  check_eq(p("12.3ms"), res_t{pec::fractional_timespan});
  check_eq(p("12.3n"), res_t{pec::fractional_timespan});
  check_eq(p("12.3ns"), res_t{pec::fractional_timespan});
  check_eq(p("12.3mi"), res_t{pec::fractional_timespan});
  check_eq(p("12.3min"), res_t{pec::fractional_timespan});
  check_eq(p("123ss"), res_t{pec::trailing_character});
  check_eq(p("123m"), res_t{pec::unexpected_eof});
  check_eq(p("123mi"), res_t{pec::unexpected_eof});
  check_eq(p("123u"), res_t{pec::unexpected_eof});
  check_eq(p("123n"), res_t{pec::unexpected_eof});
}

} // WITH_FIXTURE(fixture)

CAF_TEST_MAIN()
