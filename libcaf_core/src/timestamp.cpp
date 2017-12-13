/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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

#include "caf/timestamp.hpp"

namespace caf {

timestamp make_timestamp() {
  return clock_source::now();
}

std::string timestamp_to_string(const timestamp& x) {
  return std::to_string(x.time_since_epoch().count());
}

void append_timestamp_to_string(std::string& x, const timestamp& y) {
  x += std::to_string(y.time_since_epoch().count());
}

int64_t timestamp_ago_ns(const timestamp& ts) {
  return (make_timestamp() - ts).count();
}

} // namespace caf
