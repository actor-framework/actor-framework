// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/io/network/ip_endpoint.hpp"
#include "caf/io/network/native_socket.hpp"

#include "caf/detail/io_export.hpp"

namespace caf::policy {

/// Policy object for wrapping default UDP operations.
struct CAF_IO_EXPORT udp {
  /// Write a datagram containing `buf_len` bytes to `fd` addressed
  /// at the endpoint in `sa` with size `sa_len`. Returns true as long
  /// as no IO error occurs. The number of written bytes is stored in
  /// `result` and the sender is stored in `ep`.
  static bool read_datagram(size_t& result, io::network::native_socket fd,
                            void* buf, size_t buf_len,
                            io::network::ip_endpoint& ep);

  /// Reveice a datagram of up to `len` bytes. Larger datagrams are truncated.
  /// Up to `sender_len` bytes of the receiver address is written into
  /// `sender_addr`. Returns `true` if no IO error occurred. The number of
  /// received bytes is stored in `result` (can be 0).
  static bool write_datagram(size_t& result, io::network::native_socket fd,
                             void* buf, size_t buf_len,
                             const io::network::ip_endpoint& ep);

  /// Always returns `false`. Native UDP I/O event handlers only rely on the
  /// socket buffer.
  static constexpr bool must_read_more(io::network::native_socket, size_t) {
    return false;
  }
};

} // namespace caf::policy
