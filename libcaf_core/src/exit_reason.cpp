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

#include "caf/exit_reason.hpp"

namespace caf {
namespace exit_reason {

namespace {
constexpr const char* s_names_table[] = {
  "not_exited",
  "normal",
  "unhandled_exception",
  "-invalid-",
  "unhandled_sync_failure",
  "-invalid-",
  "unknown",
  "out_of_workers"
};
} // namespace <anonymous>

const char* as_string(uint32_t value) {
  if (value <= out_of_workers) {
    return s_names_table[value];
  }
  switch (value) {
    case user_shutdown: return "user_shutdown";
    case kill: return "kill";
    case remote_link_unreachable: return "remote_link_unreachable";
    default:
      if (value < user_defined) {
        return "-invalid-";
      }
      return "user_defined";
  }
}

} // namespace exit_reason
} // namespace caf
