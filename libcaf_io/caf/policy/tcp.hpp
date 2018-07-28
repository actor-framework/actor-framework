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

#include "caf/io/network/rw_state.hpp"
#include "caf/io/network/native_socket.hpp"

namespace caf {
namespace policy {

/// Policy object for wrapping default TCP operations.
struct tcp {
  /// Reads up to `len` bytes from `fd,` writing the received data
  /// to `buf`. Returns `true` as long as `fd` is readable and `false`
  /// if the socket has been closed or an IO error occured. The number
  /// of read bytes is stored in `result` (can be 0).
  static io::network::rw_state read_some(size_t& result,
                                         io::network::native_socket fd,
                                         void* buf, size_t len);

  /// Writes up to `len` bytes from `buf` to `fd`.
  /// Returns `true` as long as `fd` is readable and `false`
  /// if the socket has been closed or an IO error occured. The number
  /// of written bytes is stored in `result` (can be 0).
  static io::network::rw_state write_some(size_t& result,
                                          io::network::native_socket fd,
                                          const void* buf, size_t len);

  /// Tries to accept a new connection from `fd`. On success,
  /// the new connection is stored in `result`. Returns true
  /// as long as
  static bool try_accept(io::network::native_socket& result,
                         io::network::native_socket fd);

  /// Always returns `false`. Native TCP I/O event handlers only rely on the
  /// socket buffer.
  static constexpr bool must_read_more(io::network::native_socket, size_t) {
    return false;
  }
};

} // namespace policy
} // namespace caf
