// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/fwd.hpp"

#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"

#include <cstdint>

namespace caf::net::octet_stream {

/// Configures a @ref transport object. The default implementation simply
/// forwards to the free functions of @ref stream_socket.
class CAF_NET_EXPORT policy {
public:
  virtual ~policy();

  /// Returns the handle for the managed socket.
  virtual stream_socket handle() const = 0;

  /// Reads data from the socket into the buffer.
  virtual ptrdiff_t read(byte_span buf) = 0;

  /// Writes data from the buffer to the socket.
  virtual ptrdiff_t write(const_byte_span buf) = 0;

  /// Returns the last socket error on this thread.
  virtual errc last_error(ptrdiff_t) = 0;

  /// Checks whether connecting a non-blocking socket was successful.
  virtual ptrdiff_t connect() = 0;

  /// Convenience function that always returns 1. Exists to make writing code
  /// against multiple policies easier by providing the same interface.
  virtual ptrdiff_t accept() = 0;

  /// Returns the number of bytes that are buffered internally and available
  /// for immediate read.
  virtual size_t buffered() const noexcept = 0;
};

} // namespace caf::net::octet_stream
