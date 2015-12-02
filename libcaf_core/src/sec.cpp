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

const char* to_string(sec x) {
  switch (x) {
    case sec::unexpected_message:
      return "unexpected_message";
    case sec::no_actor_to_unpublish:
      return "no_actor_to_unpublish";
    case sec::no_actor_published_at_port:
      return "no_actor_published_at_port";
    case sec::state_not_serializable:
      return "state_not_serializable";
    case sec::invalid_sys_key:
      return "invalid_sys_key";
    case sec::disconnect_during_handshake:
      return "disconnect_during_handshake";
    case sec::cannot_forward_to_invalid_actor:
      return "cannot_forward_to_invalid_actor";
    case sec::no_route_to_receiving_node:
      return "no_route_to_receiving_node";
    case sec::failed_to_assign_scribe_from_handle:
      return "failed_to_assign_scribe_from_handle";
    case sec::cannot_close_invalid_port:
      return "cannot_close_invalid_port";
    case sec::cannot_connect_to_node:
      return "cannot_connect_to_node";
    case sec::cannot_open_port:
      return "cannot_open_port";
    case sec::cannot_spawn_actor_from_arguments:
      return "cannot_spawn_actor_from_arguments";
    default:
      return "<unknown>";
  }
}

error make_error(sec x) {
  return {static_cast<uint8_t>(x), atom("system")};
}

} // namespace caf
