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

#pragma once

#include "caf/sec.hpp"
#include "caf/detail/io_export.hpp"

namespace caf::io::basp {

/// @addtogroup BASP

/// Denotes the state of a connection between two BASP nodes. Overlaps with
/// `sec` (these states get converted to an error by the BASP instance).
enum connection_state {
  /// Indicates that a connection is established and this node is
  /// waiting for the next BASP header.
  await_header,
  /// Indicates that this node has received a header with non-zero payload
  /// and is waiting for the data.
  await_payload,
  /// Indicates that this connection no longer exists.
  close_connection,
  /// See `sec::incompatible_versions`.
  incompatible_versions,
  /// See `sec::incompatible_application_ids`.
  incompatible_application_ids,
  /// See `sec::malformed_basp_message`.
  malformed_basp_message,
  /// See `sec::serializing_basp_payload_failed`.
  serializing_basp_payload_failed,
  /// See `sec::redundant_connection`.
  redundant_connection,
  /// See `sec::no_route_to_receiving_node`.
  no_route_to_receiving_node,
};

/// Returns whether the connection state requries a shutdown of the socket
/// connection.
/// @relates connection_state
inline bool requires_shutdown(connection_state x) {
  // Any enum value other than await_header (0) and await_payload (1) signal the
  // BASP broker to shutdown the connection.
  return static_cast<int>(x) > 1;
}

/// Converts the connection state to a system error code if it holds one of the
/// overlapping values. Otherwise returns `sec::none`.
/// @relates connection_state
inline sec to_sec(connection_state x) {
  switch (x) {
    default:
      return sec::none;
    case incompatible_versions:
      return sec::incompatible_versions;
    case incompatible_application_ids:
      return sec::incompatible_application_ids;
    case malformed_basp_message:
      return sec::malformed_basp_message;
    case serializing_basp_payload_failed:
      return sec::serializing_basp_payload_failed;
    case redundant_connection:
      return sec::redundant_connection;
    case no_route_to_receiving_node:
      return sec::no_route_to_receiving_node;
  }
}

/// @relates connection_state
CAF_IO_EXPORT std::string to_string(connection_state x);

/// @}

} // namespace caf::io::basp
