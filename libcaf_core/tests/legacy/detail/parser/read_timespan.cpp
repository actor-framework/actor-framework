// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE detail.parser.read_timespan

#include "caf/detail/parser/read_timespan.hpp"

#include "caf/parser_state.hpp"

#include "core-test.hpp"

#include <chrono>
#include <string_view>

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

std::optional<timespan> read(std::string_view str) {
  timespan_consumer consumer;
  string_parser_state ps{str.begin(), str.end()};
  detail::parser::read_timespan(ps, consumer);
  if (ps.code != pec::success)
    return std::nullopt;
  return consumer.x;
}

} // namespace

CAF_TEST(todo) {
  CHECK_EQ(read("12ns"), 12_ns);
  CHECK_EQ(read("34us"), 34_us);
  CHECK_EQ(read("56ms"), 56_ms);
  CHECK_EQ(read("78s"), 78_s);
  CHECK_EQ(read("60min"), 1_h);
  CHECK_EQ(read("90h"), 90_h);
}
