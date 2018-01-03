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

#include "caf/sec.hpp"

#include "caf/detail/enum_to_string.hpp"

namespace caf {

namespace {

const char* sec_strings[] = {
  "none",
  "unexpected_message",
  "unexpected_response",
  "request_receiver_down",
  "request_timeout",
  "no_such_group_module",
  "no_actor_published_at_port",
  "unexpected_actor_messaging_interface",
  "state_not_serializable",
  "unsupported_sys_key",
  "unsupported_sys_message",
  "disconnect_during_handshake",
  "cannot_forward_to_invalid_actor",
  "no_route_to_receiving_node",
  "failed_to_assign_scribe_from_handle",
  "failed_to_assign_doorman_from_handle",
  "cannot_close_invalid_port",
  "cannot_connect_to_node",
  "cannot_open_port",
  "network_syscall_failed",
  "invalid_argument",
  "invalid_protocol_family",
  "cannot_publish_invalid_actor",
  "cannot_spawn_actor_from_arguments",
  "end_of_stream",
  "no_context",
  "unknown_type",
  "no_proxy_registry",
  "runtime_error",
  "remote_linking_failed",
  "cannot_add_upstream",
  "upstream_already_exists",
  "invalid_upstream",
  "cannot_add_downstream",
  "downstream_already_exists",
  "invalid_downstream",
  "no_downstream_stages_defined",
  "stream_init_failed",
  "invalid_stream_state",
  "bad_function_call",
  "feature_disabled",
};

} // namespace <anonymous>

std::string to_string(sec x) {
  return detail::enum_to_string(x, sec_strings);
}

error make_error(sec x) {
  return {static_cast<uint8_t>(x), atom("system")};
}

} // namespace caf
