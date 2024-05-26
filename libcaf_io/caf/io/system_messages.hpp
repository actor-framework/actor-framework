// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/io/accept_handle.hpp"
#include "caf/io/connection_handle.hpp"
#include "caf/io/datagram_handle.hpp"
#include "caf/io/handle.hpp"
#include "caf/io/network/receive_buffer.hpp"

#include "caf/byte_buffer.hpp"

#include <iomanip>
#include <sstream>
#include <tuple>
#include <vector>

namespace caf::io {

/// Signalizes a newly accepted connection from a {@link broker}.
struct new_connection_msg {
  /// The handle that accepted the new connection.
  accept_handle source;
  /// The handle for the new connection.
  connection_handle handle;
};

template <class Inspector>
bool inspect(Inspector& f, new_connection_msg& x) {
  return f.object(x).fields(f.field("source", x.source),
                            f.field("handle", x.handle));
}

/// Signalizes newly arrived data for a {@link broker}.
struct new_data_msg {
  /// Handle to the related connection.
  connection_handle handle;
  /// Buffer containing the received data.
  byte_buffer buf;
};

/// @relates new_data_msg
template <class Inspector>
bool inspect(Inspector& f, new_data_msg& x) {
  return f.object(x).fields(f.field("handle", x.handle), f.field("buf", x.buf));
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
bool inspect(Inspector& f, data_transferred_msg& x) {
  return f.object(x).fields(f.field("handle", x.handle),
                            f.field("written", x.written),
                            f.field("remaining", x.remaining));
}

/// Signalizes that a {@link broker} connection has been closed.
struct connection_closed_msg {
  /// Handle to the closed connection.
  connection_handle handle;
};

/// @relates connection_closed_msg
template <class Inspector>
bool inspect(Inspector& f, connection_closed_msg& x) {
  return f.object(x).fields(f.field("handle", x.handle));
}

/// Signalizes that a {@link broker} acceptor has been closed.
struct acceptor_closed_msg {
  /// Handle to the closed connection.
  accept_handle handle;
};

/// @relates connection_closed_msg
template <class Inspector>
bool inspect(Inspector& f, acceptor_closed_msg& x) {
  return f.object(x).fields(f.field("handle", x.handle));
}

/// Signalizes that a connection has entered passive mode.
struct connection_passivated_msg {
  connection_handle handle;
};

/// @relates connection_passivated_msg
template <class Inspector>
bool inspect(Inspector& f, connection_passivated_msg& x) {
  return f.object(x).fields(f.field("handle", x.handle));
}

/// Signalizes that an acceptor has entered passive mode.
struct acceptor_passivated_msg {
  accept_handle handle;
};

/// @relates acceptor_passivated_msg
template <class Inspector>
bool inspect(Inspector& f, acceptor_passivated_msg& x) {
  return f.object(x).fields(f.field("handle", x.handle));
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
bool inspect(Inspector& f, new_datagram_msg& x) {
  return f.object(x).fields(f.field("handle", x.handle), f.field("buf", x.buf));
}

/// Signalizes that a datagram with a certain size has been sent.
struct datagram_sent_msg {
  // Handle to the endpoint used.
  datagram_handle handle;
  // Number of bytes written.
  uint64_t written;
  // Buffer of the sent datagram, for reuse.
  byte_buffer buf;
};

/// @relates datagram_sent_msg
template <class Inspector>
bool inspect(Inspector& f, datagram_sent_msg& x) {
  return f.object(x).fields(f.field("handle", x.handle),
                            f.field("written", x.written),
                            f.field("buf", x.buf));
}

/// Signalizes that a datagram sink has entered passive mode.
struct datagram_servant_passivated_msg {
  datagram_handle handle;
};

/// @relates datagram_servant_passivated_msg
template <class Inspector>
bool inspect(Inspector& f, datagram_servant_passivated_msg& x) {
  return f.object(x).fields(f.field("handle", x.handle));
}

/// Signalizes that a datagram endpoint has entered passive mode.
struct datagram_servant_closed_msg {
  std::vector<datagram_handle> handles;
};

/// @relates datagram_servant_closed_msg
template <class Inspector>
bool inspect(Inspector& f, datagram_servant_closed_msg& x) {
  return f.object(x).fields(f.field("handles", x.handles));
}

} // namespace caf::io
