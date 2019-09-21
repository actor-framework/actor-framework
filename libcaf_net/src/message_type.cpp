/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/net/basp/message_type.hpp"

#include "caf/string_view.hpp"

namespace caf {
namespace net {
namespace basp {

namespace {

string_view message_type_names[] = {
  "handshake",       "actor_message", "resolve_request", "resolve_response",
  "monitor_message", "down_message",  "heartbeat",
};

} // namespace

std::string to_string(message_type x) {
  auto result = message_type_names[static_cast<uint8_t>(x)];
  return std::string{result.begin(), result.end()};
}

} // namespace basp
} // namespace net
} // namespace caf
