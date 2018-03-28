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

#include <tuple>
#include <vector>
#include <sstream>
#include <iomanip>

#include "caf/meta/type_name.hpp"
#include "caf/meta/hex_formatted.hpp"

#include "caf/io/handle.hpp"
#include "caf/io/accept_handle.hpp"
#include "caf/io/datagram_handle.hpp"
#include "caf/io/connection_handle.hpp"
#include "caf/io/network/receive_buffer.hpp"

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
typename Inspector::result_type inspect(Inspector& f, new_connection_msg& x) {
  return f(meta::type_name("new_connection_msg"), x.source, x.handle);
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
typename Inspector::result_type inspect(Inspector& f, new_data_msg& x) {
  return f(meta::type_name("new_data_msg"), x.handle,
           meta::hex_formatted(), x.buf);
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
typename Inspector::result_type inspect(Inspector& f, data_transferred_msg& x) {
  return f(meta::type_name("data_transferred_msg"),
           x.handle, x.written, x.remaining);
}

/// Signalizes that a {@link broker} connection has been closed.
struct connection_closed_msg {
  /// Handle to the closed connection.
  connection_handle handle;
};

/// @relates connection_closed_msg
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, connection_closed_msg& x) {
  return f(meta::type_name("connection_closed_msg"), x.handle);
}

/// Signalizes that a {@link broker} acceptor has been closed.
struct acceptor_closed_msg {
  /// Handle to the closed connection.
  accept_handle handle;
};

/// @relates connection_closed_msg
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, acceptor_closed_msg& x) {
  return f(meta::type_name("acceptor_closed_msg"), x.handle);
}

/// Signalizes that a connection has entered passive mode.
struct connection_passivated_msg {
  connection_handle handle;
};

/// @relates connection_passivated_msg
template <class Inspector>
typename Inspector::result_type
inspect(Inspector& f, connection_passivated_msg& x) {
  return f(meta::type_name("connection_passivated_msg"), x.handle);
}

/// Signalizes that an acceptor has entered passive mode.
struct acceptor_passivated_msg {
  accept_handle handle;
};

/// @relates acceptor_passivated_msg
template <class Inspector>
typename Inspector::result_type
inspect(Inspector& f, acceptor_passivated_msg& x) {
  return f(meta::type_name("acceptor_passivated_msg"), x.handle);
}

/// Signalizes that a datagram with a certain size has been sent.
struct new_datagram_msg {
  // Handle to the endpoint used.
  datagram_handle handle;
  // Buffer containing received data.
  network::receive_buffer buf;
};

/// @relates new_datagram_msg
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, new_datagram_msg& x) {
  return f(meta::type_name("new_datagram_msg"), x.handle, x.buf);
}

/// Signalizes that a datagram with a certain size has been sent.
struct datagram_sent_msg {
  // Handle to the endpoint used.
  datagram_handle handle;
  // Number of bytes written.
  uint64_t written;
  // Buffer of the sent datagram, for reuse.
  std::vector<char> buf;
};

/// @relates datagram_sent_msg
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, datagram_sent_msg& x) {
  return f(meta::type_name("datagram_sent_msg"), x.handle, x.written, x.buf);
}

/// Signalizes that a datagram sink has entered passive mode.
struct datagram_servant_passivated_msg {
  datagram_handle handle;
};

/// @relates datagram_servant_passivated_msg
template <class Inspector>
typename Inspector::result_type
inspect(Inspector& f, datagram_servant_passivated_msg& x) {
  return f(meta::type_name("datagram_servant_passivated_msg"), x.handle);
}

/// Signalizes that a datagram endpoint has entered passive mode.
struct datagram_servant_closed_msg {
  std::vector<datagram_handle> handles;
};

/// @relates datagram_servant_closed_msg
template <class Inspector>
typename Inspector::result_type
inspect(Inspector& f, datagram_servant_closed_msg& x) {
  return f(meta::type_name("datagram_servant_closed_msg"), x.handles);
}

} // namespace io
} // namespace caf

