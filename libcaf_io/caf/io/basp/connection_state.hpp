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


namespace caf {
namespace io {
namespace basp {

/// @addtogroup BASP

/// Denotes the state of a connection between to BASP nodes.
enum connection_state {
  /// Indicates that a connection is established and this node is
  /// waiting for the next BASP header.
  await_header,
  /// Indicates that this node has received a header with non-zero payload
  /// and is waiting for the data.
  await_payload,
  /// Indicates that this connection no longer exists.
  close_connection
};

/// @relates connection_state
inline std::string to_string(connection_state x) {
  return x == await_header ? "await_header"
                           : (x == await_payload ? "await_payload"
                                                 : "close_connection");
}

/// @}

} // namespace basp
} // namespace io
} // namespace caf


