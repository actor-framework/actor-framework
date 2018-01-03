/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include <sstream>

#include "caf/duration.hpp"

#include "caf/detail/enum_to_string.hpp"

namespace caf {

namespace {

const char* time_unit_strings[] = {
  "invalid",
  "minutes",
  "seconds",
  "milliseconds",
  "microseconds",
  "nanoseconds"
};

const char* time_unit_short_strings[] = {
  "?",
  "min",
  "s",
  "ms",
  "us",
  "ns"
};

} // namespace <anonymous>

std::string to_string(time_unit x) {
  return detail::enum_to_string(x, time_unit_strings);
}

std::string to_string(const duration& x) {
  if (x.unit == time_unit::invalid)
    return "infinite";
  auto result = std::to_string(x.count);
  result += detail::enum_to_string(x.unit, time_unit_short_strings);
  return result;
}

bool operator==(const duration& lhs, const duration& rhs) {
  return lhs.unit == rhs.unit && lhs.count == rhs.count;
}

} // namespace caf
