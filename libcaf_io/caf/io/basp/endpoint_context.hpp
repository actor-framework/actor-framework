// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/io/basp/connection_state.hpp"
#include "caf/io/basp/header.hpp"
#include "caf/io/connection_handle.hpp"
#include "caf/io/datagram_handle.hpp"

#include "caf/actor_clock.hpp"
#include "caf/response_promise.hpp"
#include "caf/timestamp.hpp"

#include <unordered_map>

namespace caf::io::basp {

// stores meta information for active endpoints
struct endpoint_context {
  // denotes what message we expect from the remote node next
  basp::connection_state cstate;
  // our currently processed BASP header
  basp::header hdr;
  // the handle for I/O operations
  connection_handle hdl;
  // network-agnostic node identifier
  node_id id;
  // ports
  uint16_t remote_port;
  uint16_t local_port;
  // pending operations to be performed after handshake completed
  std::optional<response_promise> callback;
  // keeps track of when we've last received a message from this endpoint
  actor_clock::time_point last_seen;
};

} // namespace caf::io::basp
