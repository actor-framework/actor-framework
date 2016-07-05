/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_IO_SYSTEM_MESSAGES_HPP
#define CAF_IO_SYSTEM_MESSAGES_HPP

#include <tuple>
#include <vector>
#include <sstream>
#include <iomanip>

#include "caf/meta/type_name.hpp"

#include "caf/io/handle.hpp"
#include "caf/io/accept_handle.hpp"
#include "caf/io/connection_handle.hpp"

namespace caf {
namespace io {

/// Signalizes a newly accepted connection from a {@link broker}.
struct new_connection_msg {
  /// The handle that accepted the new connection.
  accept_handle source;
  /// The handle for the new connection.
  connection_handle handle;
};

template <class Inspector>
error inspect(Inspector& f, new_connection_msg& x) {
  return f(meta::type_name("new_connection_msg"), x.source, x.handle);
}

/// @relates new_connection_msg
inline bool operator==(const new_connection_msg& lhs,
                       const new_connection_msg& rhs) {
  return lhs.source == rhs.source && lhs.handle == rhs.handle;
}

/// @relates new_connection_msg
inline bool operator!=(const new_connection_msg& lhs,
                       const new_connection_msg& rhs) {
  return !(lhs == rhs);
}

/// Signalizes newly arrived data for a {@link broker}.
struct new_data_msg {
  /// Handle to the related connection.
  connection_handle handle;
  /// Buffer containing the received data.
  std::vector<char> buf;
};

/// @relates new_data_msg
template <class Inspector>
error inspect(Inspector& f, new_data_msg& x) {
  return f(meta::type_name("new_data_msg"), x.handle, x.buf);
}

/// @relates new_data_msg
inline bool operator==(const new_data_msg& lhs, const new_data_msg& rhs) {
  return lhs.handle == rhs.handle && lhs.buf == rhs.buf;
}

/// @relates new_data_msg
inline bool operator!=(const new_data_msg& lhs, const new_data_msg& rhs) {
  return !(lhs == rhs);
}

/// Signalizes that a certain amount of bytes has been written.
struct data_transferred_msg {
  /// Handle to the related connection.
  connection_handle handle;
  /// Number of transferred bytes.
  uint64_t written;
  /// Number of remaining bytes in all send buffers.
  uint64_t remaining;
};

/// @relates data_transferred_msg
template <class Inspector>
error inspect(Inspector& f, data_transferred_msg& x) {
  return f(meta::type_name("data_transferred_msg"),
           x.handle, x.written, x.remaining);
}

/// @relates data_transferred_msg
inline bool operator==(const data_transferred_msg& x,
                       const data_transferred_msg& y) {
  return x.handle == y.handle
      && x.written == y.written
      && x.remaining == y.remaining;
}

/// @relates data_transferred_msg
inline bool operator!=(const data_transferred_msg& x,
                       const data_transferred_msg& y) {
  return ! (x == y);
}


/// Signalizes that a {@link broker} connection has been closed.
struct connection_closed_msg {
  /// Handle to the closed connection.
  connection_handle handle;
};

/// @relates connection_closed_msg
template <class Inspector>
error inspect(Inspector& f, connection_closed_msg& x) {
  return f(meta::type_name("connection_closed_msg"), x.handle);
}

/// @relates connection_closed_msg
inline bool operator==(const connection_closed_msg& lhs,
                       const connection_closed_msg& rhs) {
  return lhs.handle == rhs.handle;
}

/// @relates connection_closed_msg
inline bool operator!=(const connection_closed_msg& lhs,
                       const connection_closed_msg& rhs) {
  return !(lhs == rhs);
}

/// Signalizes that a {@link broker} acceptor has been closed.
struct acceptor_closed_msg {
  /// Handle to the closed connection.
  accept_handle handle;
};

/// @relates connection_closed_msg
template <class Inspector>
error inspect(Inspector& f, acceptor_closed_msg& x) {
  return f(meta::type_name("acceptor_closed_msg"), x.handle);
}

/// @relates acceptor_closed_msg
inline bool operator==(const acceptor_closed_msg& lhs,
                       const acceptor_closed_msg& rhs) {
  return lhs.handle == rhs.handle;
}

/// @relates acceptor_closed_msg
inline bool operator!=(const acceptor_closed_msg& lhs,
                       const acceptor_closed_msg& rhs) {
  return !(lhs == rhs);
}

} // namespace io
} // namespace caf

#endif // CAF_IO_SYSTEM_MESSAGES_HPP
