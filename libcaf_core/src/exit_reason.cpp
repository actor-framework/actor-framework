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

const char* to_string(exit_reason x) {
  switch (x) {
    case exit_reason::not_exited:
      return "not_exited";
    case exit_reason::normal:
      return "normal";
    case exit_reason::unhandled_exception:
      return "unhandled_exception";
    case exit_reason::unhandled_sync_failure:
      return "unhandled_sync_failure";
    case exit_reason::unknown:
      return "unknown";
    case exit_reason::out_of_workers:
      return "out_of_workers";
    case exit_reason::user_shutdown:
      return "user_shutdown";
    case exit_reason::kill:
      return "kill";
    case exit_reason::remote_link_unreachable:
      return "remote_link_unreachable";
    default:
      return "-invalid-";
  }
}

} // namespace caf
