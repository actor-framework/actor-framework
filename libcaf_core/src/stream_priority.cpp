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

#include "caf/stream_priority.hpp"

namespace caf {

std::string to_string(stream_priority x) {
  switch (x) {
    default:
      return "invalid";
    case stream_priority::very_high:
      return "very_high";
    case stream_priority::high:
      return "high";
    case stream_priority::normal:
      return "normal";
    case stream_priority::low:
      return "low";
    case stream_priority::very_low:
      return "very_low";
  }
}

} // namespace caf
