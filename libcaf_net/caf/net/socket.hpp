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
#include <system_error>
#include <type_traits>

#include "caf/config.hpp"
#include "caf/fwd.hpp"
#include "caf/net/abstract_socket.hpp"
#include "caf/net/socket_id.hpp"

namespace caf {
namespace net {

/// An internal endpoint for sending or receiving data. Can be either a
/// ::network_socket or a ::pipe_socket.
struct socket : abstract_socket<socket> {
  using super = abstract_socket<socket>;

  using super::super;
};

/// Denotes the invalid socket.
constexpr auto invalid_socket = socket{invalid_socket_id};

/// Converts between different socket types.
template <class To, class From>
To socket_cast(From x) {
  return To{x.id};
}

/// Close socket `x`.
/// @relates socket
void close(socket x);

/// Returns the last socket error in this thread as an integer.
/// @relates socket
std::errc last_socket_error();

/// Returns the last socket error as human-readable string.
/// @relates socket
std::string last_socket_error_as_string();

/// Sets x to be inherited by child processes if `new_value == true`
/// or not if `new_value == false`.  Not implemented on Windows.
/// @relates socket
error child_process_inherit(socket x, bool new_value);

/// Enables or disables nonblocking I/O on `x`.
/// @relates socket
error nonblocking(socket x, bool new_value);

} // namespace net
} // namespace caf
