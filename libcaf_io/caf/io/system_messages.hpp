/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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
#include "caf/io/dgram_scribe_handle.hpp"
#include "caf/io/dgram_doorman_handle.hpp"

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
  return f(meta::type_name("new_data_msg"), x.handle, x.buf);
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

/// Signalizes that a {@link broker} datagram sink was closed.
struct dgram_scribe_closed_msg {
  dgram_scribe_handle handle;
};

/// @relates dgram_scribe_closed_msg
template <class Inspector>
typename Inspector::result_type
inspect(Inspector& f, dgram_scribe_closed_msg& x) {
  return f(meta::type_name("dgram_scribe_closed_msg"), x.handle);
}

/// Signalizes that a {@link broker} datagram source was closed.
struct dgram_doorman_closed_msg {
  dgram_doorman_handle handle;
};

/// @relates dgram_doorman_closed_msg
template <class Inspector>
typename Inspector::result_type
inspect(Inspector& f, dgram_doorman_closed_msg& x) {
  return f(meta::type_name("dgram_doorman_closed_msg"), x.handle);
}

/// Signalizes newly arrived datagram for a {@link broker}.
struct new_endpoint_msg {
  /// Handle to the related datagram endpoint.
  dgram_doorman_handle handle;
  // Buffer containing the received data.
  // std::vector<char> buf;
  /// Handle to new dgram_scribe
  dgram_scribe_handle endpoint;
};

/// @relates new_endpoint_msg
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, new_endpoint_msg& x) {
  return f(meta::type_name("new_endpoint_msg"), x.handle, x.endpoint);
}

/// Signalizes that a datagram with a certain size has been sent.
struct new_datagram_msg {
  // Handle to the endpoint used.
  dgram_scribe_handle handle;
  // Buffer containing received data.
  std::vector<char> buf;
};

/// @relates new_datagram_msg
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, new_datagram_msg& x) {
  return f(meta::type_name("new_datagram_msg"), x.handle, x.buf);
}

/// Signalizes that a datagram with a certain size has been sent.
struct datagram_sent_msg {
  // Handle to the endpoint used.
  dgram_scribe_handle handle;
  // number of bytes written.
  uint64_t written;
};

/// @relates datagram_sent_msg
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, datagram_sent_msg& x) {
  return f(meta::type_name("datagram_sent_msg"), x.handle, x.written);
}

/// Signalizes that a datagram sink has entered passive mode.
struct dgram_scribe_passivated_msg {
  dgram_scribe_handle handle;
};

/// @relates dgram_scribe_passivated_msg
template <class Inspector>
typename Inspector::result_type
inspect(Inspector& f, dgram_scribe_passivated_msg& x) {
  return f(meta::type_name("dgram_scribe_passivated_msg"), x.handle);
}

/// Signalizes that a datagram source has entered passive mode.
struct dgram_doorman_passivated_msg {
  dgram_doorman_handle handle;
};

/// @relates dgram_doorman_passivated_msg
template <class Inspector>
typename Inspector::result_type
inspect(Inspector& f, dgram_doorman_passivated_msg& x) {
  return f(meta::type_name("dgram_doorman_passivated_msg"), x.handle);
}

} // namespace io
} // namespace caf

#endif // CAF_IO_SYSTEM_MESSAGES_HPP
