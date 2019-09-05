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

#pragma once

#include <string>

namespace caf {
namespace net {
namespace basp {

/// @addtogroup BASP

/// Stores the state of a connection in a `basp::application`.
enum class connection_state {
  /// Indicates that we have just accepted or opened a connection and await the
  /// magic number.
  await_magic_number,
  /// Indicates that we successfully checked the magic number and now wait for
  /// the handshake header.
  await_handshake_header,
  /// Indicates that we received the header for the handshake and now wait for
  /// the payload.
  await_handshake_payload,
  /// Indicates that a connection is established and this node is waiting for
  /// the next BASP header.
  await_header,
  /// Indicates that this node has received a header with non-zero payload and
  /// is waiting for the data.
  await_payload,
  /// Indicates that we are about to close this connection.
  shutdown,
};

/// @relates connection_state
std::string to_string(connection_state x);

/// @}

} // namespace basp
} // namespace net
} // namespace caf
