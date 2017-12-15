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

#include "caf/instrumentation/callsite_stats.hpp"

namespace caf {
namespace instrumentation {

void callsite_stats::record_pre_behavior(int64_t mb_wait_time, size_t mb_size) {
  mb_waittimes_.record(mb_wait_time);
  mb_sizes_.record(mb_size);
}

std::string callsite_stats::to_string() const {
  return std::string("WAIT ") + mb_waittimes_.to_string()
         + " | " + " SIZE " + mb_sizes_.to_string();
}

} // namespace instrumentation
} // namespace caf
