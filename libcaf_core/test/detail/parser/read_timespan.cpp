// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE detail.parser.read_timespan

#include "caf/detail/parser/read_timespan.hpp"

#include "caf/test/dsl.hpp"

#include <chrono>

#include "caf/parser_state.hpp"
#include "caf/string_view.hpp"
#include "caf/variant.hpp"

using namespace caf;

namespace {

using std::chrono::duration_cast;

timespan operator"" _ns(unsigned long long x) {
  return duration_cast<timespan>(std::chrono::nanoseconds(x));
}

timespan operator"" _us(unsigned long long x) {
  return duration_cast<timespan>(std::chrono::microseconds(x));
}

timespan operator"" _ms(unsigned long long x) {
  return duration_cast<timespan>(std::chrono::milliseconds(x));
}

timespan operator"" _s(unsigned long long x) {
  return duration_cast<timespan>(std::chrono::seconds(x));
}

timespan operator"" _h(unsigned long long x) {
  return duration_cast<timespan>(std::chrono::hours(x));
}

struct timespan_consumer {
  using value_type = timespan;

  void value(timespan y) {
    x = y;
  }

  timespan x;
};

optional<timespan> read(string_view str) {
  timespan_consumer consumer;
  string_parser_state ps{str.begin(), str.end()};
  detail::parser::read_timespan(ps, consumer);
  if (ps.code != pec::success)
    return none;
  return consumer.x;
}

} // namespace

CAF_TEST(todo) {
  CAF_CHECK_EQUAL(read("12ns"), 12_ns);
  CAF_CHECK_EQUAL(read("34us"), 34_us);
  CAF_CHECK_EQUAL(read("56ms"), 56_ms);
  CAF_CHECK_EQUAL(read("78s"), 78_s);
  CAF_CHECK_EQUAL(read("60min"), 1_h);
  CAF_CHECK_EQUAL(read("90h"), 90_h);
}
