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

#include "caf/sec.hpp"

namespace caf {

namespace {

const char* sec_strings[] = {
  "<no-error>",
  "unexpected_message",
  "unexpected_response",
  "request_receiver_down",
  "request_timeout",
  "no_actor_published_at_port",
  "state_not_serializable",
  "unsupported_sys_key",
  "unsupported_sys_message",
  "disconnect_during_handshake",
  "cannot_forward_to_invalid_actor",
  "no_route_to_receiving_node",
  "failed_to_assign_scribe_from_handle",
  "cannot_close_invalid_port",
  "cannot_connect_to_node",
  "cannot_open_port",
  "cannot_spawn_actor_from_arguments"
};

} // namespace <anonymous>

const char* to_string(sec x) {
  auto index = static_cast<size_t>(x);
  if (index > static_cast<size_t>(sec::cannot_spawn_actor_from_arguments))
    return "<unknown>";
  return sec_strings[index];
}

error make_error(sec x) {
  return {static_cast<uint8_t>(x), atom("system")};
}

error make_error(sec x, message context) {
  return {static_cast<uint8_t>(x), atom("system"), std::move(context)};
}

} // namespace caf
