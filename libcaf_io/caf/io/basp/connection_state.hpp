// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/default_enum_inspect.hpp"
#include "caf/detail/io_export.hpp"
#include "caf/sec.hpp"

#include <cstdint>
#include <string>
#include <type_traits>

namespace caf::io::basp {

/// @addtogroup BASP
/// @{

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
  /// See `sec::malformed_message`.
  malformed_message,
  /// See `sec::serializing_basp_payload_failed`.
  serializing_basp_payload_failed,
  /// See `sec::redundant_connection`.
  redundant_connection,
  /// See `sec::no_route_to_receiving_node`.
  no_route_to_receiving_node,
};

/// Returns whether the connection state requires a shutdown of the socket
/// connection.
constexpr bool requires_shutdown(connection_state x) noexcept {
  // Any enum value other than await_header (0) and await_payload (1) signal the
  // BASP broker to shutdown the connection.
  return static_cast<int>(x) > 1;
}

/// Converts the connection state to a system error code if it holds one of the
/// overlapping values. Otherwise returns `sec::none`.
inline sec to_sec(connection_state x) noexcept {
  switch (x) {
    default:
      return sec::none;
    case incompatible_versions:
      return sec::incompatible_versions;
    case incompatible_application_ids:
      return sec::incompatible_application_ids;
    case malformed_message:
      return sec::malformed_message;
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

/// @relates connection_state
CAF_IO_EXPORT bool from_string(std::string_view, connection_state&);

/// @relates connection_state
CAF_IO_EXPORT bool from_integer(std::underlying_type_t<connection_state>,
                                connection_state&);

/// @relates connection_state
template <class Inspector>
bool inspect(Inspector& f, connection_state& x) {
  return default_enum_inspect(f, x);
}

/// @}

} // namespace caf::io::basp
