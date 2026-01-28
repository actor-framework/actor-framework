// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/io/network/rw_state.hpp"
#include "caf/net/socket_id.hpp"

#include "caf/detail/io_export.hpp"

namespace caf::policy {

/// Policy object for wrapping default TCP operations.
struct CAF_IO_EXPORT tcp {
  /// Reads up to `len` bytes from `fd,` writing the received data
  /// to `buf`. Returns `true` as long as `fd` is readable and `false`
  /// if the socket has been closed or an IO error occurred. The number
  /// of read bytes is stored in `result` (can be 0).
  static io::network::rw_state read_some(size_t& result, net::socket_id fd,
                                         void* buf, size_t len);

  /// Writes up to `len` bytes from `buf` to `fd`.
  /// Returns `true` as long as `fd` is readable and `false`
  /// if the socket has been closed or an IO error occurred. The number
  /// of written bytes is stored in `result` (can be 0).
  static io::network::rw_state write_some(size_t& result, net::socket_id fd,
                                          const void* buf, size_t len);

  /// Tries to accept a new connection from `fd`. On success,
  /// the new connection is stored in `result`. Returns true
  /// as long as
  static bool try_accept(net::socket_id& result, net::socket_id fd);

  /// Always returns `false`. Native TCP I/O event handlers only rely on the
  /// socket buffer.
  static constexpr bool must_read_more(net::socket_id, size_t) {
    return false;
  }
};

} // namespace caf::policy
