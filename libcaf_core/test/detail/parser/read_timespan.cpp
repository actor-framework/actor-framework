/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

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
