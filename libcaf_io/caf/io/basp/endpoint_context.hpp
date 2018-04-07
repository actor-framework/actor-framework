/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include <unordered_map>

#include "caf/variant.hpp"
#include "caf/response_promise.hpp"

#include "caf/io/datagram_handle.hpp"
#include "caf/io/connection_handle.hpp"

#include "caf/io/basp/header.hpp"
#include "caf/io/basp/connection_state.hpp"

namespace caf {
namespace io {
namespace basp {

// stores meta information for active endpoints
struct endpoint_context {
  using pending_map = std::map<sequence_type, std::pair<basp::header,
                                                        std::vector<char>>>;
  // denotes what message we expect from the remote node next
  basp::connection_state cstate;
  // our currently processed BASP header
  basp::header hdr;
  // the handle for I/O operations
  variant<connection_handle, datagram_handle> hdl;
  // network-agnostic node identifier
  node_id id;
  // ports
  uint16_t remote_port;
  uint16_t local_port;
  // pending operations to be performed after handshake completed
  optional<response_promise> callback;
  // protocols that do not implement ordering are ordered by CAF
  bool requires_ordering;
  // sequence numbers and a buffer to establish order
  sequence_type seq_incoming;
  sequence_type seq_outgoing;
  // pending messages due to ordering
  pending_map pending;
  // track if a timeout to deliver pending messages is set
  bool did_set_timeout;
};

} // namespace basp
} // namespace io
} // namespace caf

