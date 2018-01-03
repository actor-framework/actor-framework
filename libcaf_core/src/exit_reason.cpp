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

#include "caf/exit_reason.hpp"

#include "caf/message.hpp"

#include "caf/detail/enum_to_string.hpp"

namespace caf {

namespace {

const char* exit_reason_strings[] = {
  "normal",
  "unhandled_exception",
  "unknown",
  "out_of_workers",
  "user_shutdown",
  "kill",
  "remote_link_unreachable",
  "unreachable"
};

} // namespace <anonymous>


std::string to_string(exit_reason x) {
  return detail::enum_to_string(x, exit_reason_strings);
}

error make_error(exit_reason x) {
  return {static_cast<uint8_t>(x), atom("exit")};
}

error make_error(exit_reason x, message context) {
  return {static_cast<uint8_t>(x), atom("exit"), std::move(context)};
}

} // namespace caf
