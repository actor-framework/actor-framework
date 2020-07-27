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
#include "caf/detail/comparable.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"
#include "caf/net/socket_id.hpp"

namespace caf::net {

/// An internal endpoint for sending or receiving data. Can be either a
/// ::network_socket, ::pipe_socket, ::stream_socket, or ::datagram_socket.
struct CAF_NET_EXPORT socket : detail::comparable<socket> {
  socket_id id;

  constexpr socket() noexcept : id(invalid_socket_id) {
    // nop
  }

  constexpr explicit socket(socket_id id) noexcept : id(id) {
    // nop
  }

  constexpr socket(const socket& other) noexcept = default;

  socket& operator=(const socket& other) noexcept = default;

  constexpr signed_socket_id compare(socket other) const noexcept {
    return static_cast<signed_socket_id>(id)
           - static_cast<signed_socket_id>(other.id);
  }
};

/// @relates socket
template <class Inspector>
typename Inspector::result_type CAF_NET_EXPORT inspect(Inspector& f,
                                                       socket& x) {
  return f(x.id);
}

/// Denotes the invalid socket.
constexpr auto invalid_socket = socket{invalid_socket_id};

/// Converts between different socket types.
template <class To, class From>
To CAF_NET_EXPORT socket_cast(From x) {
  return To{x.id};
}

/// Closes socket `x`.
/// @relates socket
void CAF_NET_EXPORT close(socket x);

/// Returns the last socket error in this thread as an integer.
/// @relates socket
std::errc CAF_NET_EXPORT last_socket_error();

/// Checks whether `last_socket_error()` would return an error code that
/// indicates a temporary error.
/// @returns `true` if `last_socket_error()` returned either
/// `std::errc::operation_would_block` or
/// `std::errc::resource_unavailable_try_again`, `false` otherwise.
/// @relates socket
bool CAF_NET_EXPORT last_socket_error_is_temporary();

/// Returns the last socket error as human-readable string.
/// @relates socket
std::string CAF_NET_EXPORT last_socket_error_as_string();

/// Sets x to be inherited by child processes if `new_value == true`
/// or not if `new_value == false`.  Not implemented on Windows.
/// @relates socket
error CAF_NET_EXPORT child_process_inherit(socket x, bool new_value);

/// Enables or disables nonblocking I/O on `x`.
/// @relates socket
error CAF_NET_EXPORT nonblocking(socket x, bool new_value);

} // namespace caf::net
