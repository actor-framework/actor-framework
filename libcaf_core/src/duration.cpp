/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENCE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include <sstream>

#include "caf/duration.hpp"

namespace caf {

namespace {

inline uint64_t ui64_val(const duration& d) {
  return static_cast<uint64_t>(d.unit) * d.count;
}

} // namespace <anonmyous>

bool operator==(const duration& lhs, const duration& rhs) {
  return (lhs.unit == rhs.unit ? lhs.count == rhs.count
                               : ui64_val(lhs) == ui64_val(rhs));
}

std::string duration::to_string() const {
  if (unit == time_unit::invalid) {
    return "-invalid-";
  }
  std::ostringstream oss;
  oss << count;
  switch (unit) {
    case time_unit::invalid:
      oss << "?";
      break;
    case time_unit::seconds:
      oss << "s";
      break;
    case time_unit::milliseconds:
      oss << "ms";
      break;
    case time_unit::microseconds:
      oss << "us";
      break;
  }
  return oss.str();
}

} // namespace caf
