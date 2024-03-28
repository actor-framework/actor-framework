// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/octet_stream/transport.hpp"
#include "caf/net/ssl/connection.hpp"

#include "caf/detail/net_export.hpp"

namespace caf::net::ssl {

/// Implements a octet stream transport that manages a stream socket with
/// encrypted communication over TLS.
class CAF_NET_EXPORT transport {
public:
  // -- member types -----------------------------------------------------------

  using worker_ptr = std::unique_ptr<socket_event_layer>;

  using connection_handle = connection;

  /// An owning smart pointer type for storing an upper layer object.
  using upper_layer_ptr = std::unique_ptr<octet_stream::upper_layer>;

  // -- factories --------------------------------------------------------------

  /// Creates a new instance of the SSL transport for a socket that has already
  /// performed the SSL handshake.
  /// @param conn The connection object for managing @p fd.
  /// @param up The layer operating on top of this transport.
  static std::unique_ptr<octet_stream::transport> make(connection conn,
                                                       upper_layer_ptr up);

  /// Returns a worker that performs the OpenSSL server handshake on the
  /// socket. On success, the worker performs a handover to an
  /// `openssl_transport` that runs `up`.
  /// @param conn The connection object for managing @p fd.
  /// @param up The layer operating on top of this transport.
  static worker_ptr make_server(connection conn, upper_layer_ptr up);

  /// Returns a worker that performs the OpenSSL client handshake on the
  /// socket. On success, the worker performs a handover to an
  /// `openssl_transport` that runs `up`.
  /// @param conn The connection object for managing @p fd.
  /// @param up The layer operating on top of this transport.
  static worker_ptr make_client(connection conn, upper_layer_ptr up);
};

} // namespace caf::net::ssl
