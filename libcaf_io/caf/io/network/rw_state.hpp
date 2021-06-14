// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

namespace caf::io::network {

/// Denotes the returned state of read and write operations on sockets.
enum class rw_state {
  /// Reports that bytes could be read or written.
  success,
  /// Reports that the socket is closed or faulty.
  failure,
  /// Reports that an empty buffer is in use and no operation was performed.
  indeterminate,
  /// Reports an ssl error: ssl_error_want_read
  ssl_error_want_read
};

} // namespace caf::io::network
