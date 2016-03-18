/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
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

namespace caf {

std::string to_string(const time_unit& x) {
  switch (x) {
    case time_unit::seconds:
      return "seconds";
    case time_unit::milliseconds:
      return "milliseconds";
    case time_unit::microseconds:
      return "microseconds";
    default:
      return "invalid";
  }
}

std::string to_string(const duration& x) {
  auto result = std::to_string(x.count);
  switch (x.unit) {
    case time_unit::seconds:
      result += "s";
      break;
    case time_unit::milliseconds:
      result += "ms";
      break;
    case time_unit::microseconds:
      result += "us";
      break;
    default:
      return "infinite";
  }
  return result;
}

bool operator==(const duration& lhs, const duration& rhs) {
  return lhs.unit == rhs.unit && lhs.count == rhs.count;
}

} // namespace caf
