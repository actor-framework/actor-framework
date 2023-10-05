// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE detail.parser.read_number_or_timespan

#include "caf/detail/parser/read_number_or_timespan.hpp"

#include "caf/parser_state.hpp"

#include "core-test.hpp"

#include <string>
#include <string_view>
#include <variant>

using namespace caf;
using namespace std::chrono;

namespace {

struct number_or_timespan_parser_consumer {
  std::variant<int64_t, double, timespan> x;
  template <class T>
  void value(T y) {
    x = y;
  }
};

struct res_t {
  std::variant<pec, double, int64_t, timespan> val;
  template <class T>
  explicit res_t(T x) : val(x) {
    // nop
  }
};

std::string to_string(const res_t& x) {
  return deep_to_string(x.val);
}

bool operator==(const res_t& x, const res_t& y) {
  if (x.val.index() != y.val.index())
    return false;
  // Implements a safe equal comparison for double.
  caf::test::equal_to f;
  using caf::get;
  using caf::holds_alternative;
  if (holds_alternative<pec>(x.val))
    return f(get<pec>(x.val), get<pec>(y.val));
  if (holds_alternative<double>(x.val))
    return f(get<double>(x.val), get<double>(y.val));
  if (holds_alternative<int64_t>(x.val))
    return f(get<int64_t>(x.val), get<int64_t>(y.val));
  return f(get<timespan>(x.val), get<timespan>(y.val));
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

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

CAF_TEST(valid numbers and timespans) {
  CHECK_EQ(p("123"), res(123));
  CHECK_EQ(p("123.456"), res(123.456));
  CHECK_EQ(p("123s"), res(seconds(123)));
  CHECK_EQ(p("123ns"), res(nanoseconds(123)));
  CHECK_EQ(p("123ms"), res(milliseconds(123)));
  CHECK_EQ(p("123us"), res(microseconds(123)));
  CHECK_EQ(p("123min"), res(minutes(123)));
}

CAF_TEST(invalid timespans) {
  CHECK_EQ(p("12.3s"), res_t{pec::fractional_timespan});
  CHECK_EQ(p("12.3n"), res_t{pec::fractional_timespan});
  CHECK_EQ(p("12.3ns"), res_t{pec::fractional_timespan});
  CHECK_EQ(p("12.3m"), res_t{pec::fractional_timespan});
  CHECK_EQ(p("12.3ms"), res_t{pec::fractional_timespan});
  CHECK_EQ(p("12.3n"), res_t{pec::fractional_timespan});
  CHECK_EQ(p("12.3ns"), res_t{pec::fractional_timespan});
  CHECK_EQ(p("12.3mi"), res_t{pec::fractional_timespan});
  CHECK_EQ(p("12.3min"), res_t{pec::fractional_timespan});
  CHECK_EQ(p("123ss"), res_t{pec::trailing_character});
  CHECK_EQ(p("123m"), res_t{pec::unexpected_eof});
  CHECK_EQ(p("123mi"), res_t{pec::unexpected_eof});
  CHECK_EQ(p("123u"), res_t{pec::unexpected_eof});
  CHECK_EQ(p("123n"), res_t{pec::unexpected_eof});
}

END_FIXTURE_SCOPE()
