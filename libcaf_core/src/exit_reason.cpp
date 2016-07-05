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

#include "caf/message.hpp"

namespace caf {

namespace {

const char* exit_reason_strings[] = {
  "normal",
  "unhandled_exception",
  "unhandled_request_error",
  "unknown",
  "out_of_workers",
  "user_shutdown",
  "kill",
  "remote_link_unreachable",
  "unreachable"
};

} // namespace <anonymous>


const char* to_string(exit_reason x) {
  auto index = static_cast<size_t>(x);
  if (index > static_cast<size_t>(exit_reason::unreachable))
    return "<unknown>";
  return exit_reason_strings[index];
}

error make_error(exit_reason x) {
  return {static_cast<uint8_t>(x), atom("exit")};
}

error make_error(exit_reason x, message context) {
  return {static_cast<uint8_t>(x), atom("exit"), std::move(context)};
}

} // namespace caf
