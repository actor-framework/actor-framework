// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/net_export.hpp"
#include "caf/net/octet_stream/transport.hpp"
#include "caf/net/ssl/connection.hpp"

namespace caf::net::ssl {

/// Implements a octet stream transport that manages a stream socket with
/// encrypted communication over TLS.
class CAF_NET_EXPORT transport : public octet_stream::transport {
public:
  // -- member types -----------------------------------------------------------

  using super = octet_stream::transport;

  using connection_handle = connection;

  using worker_ptr = std::unique_ptr<socket_event_layer>;

  class policy_impl : public octet_stream::policy {
  public:
    explicit policy_impl(connection conn);

    ptrdiff_t read(stream_socket, byte_span) override;

    ptrdiff_t write(stream_socket, const_byte_span) override;

    octet_stream::errc last_error(stream_socket, ptrdiff_t) override;

    ptrdiff_t connect(stream_socket) override;

    ptrdiff_t accept(stream_socket) override;

    size_t buffered() const noexcept override;

    connection conn;
  };

  // -- factories --------------------------------------------------------------

  /// Creates a new instance of the SSL transport for a socket that has already
  /// performed the SSL handshake.
  /// @param conn The connection object for managing @p fd.
  /// @param up The layer operating on top of this transport.
  static std::unique_ptr<transport> make(connection conn, upper_layer_ptr up);

  /// Returns a worker that performs the OpenSSL server handshake on the socket.
  /// On success, the worker performs a handover to an `openssl_transport` that
  /// runs `up`.
  /// @param conn The connection object for managing @p fd.
  /// @param up The layer operating on top of this transport.
  static worker_ptr make_server(connection conn, upper_layer_ptr up);

  /// Returns a worker that performs the OpenSSL client handshake on the socket.
  /// On success, the worker performs a handover to an `openssl_transport` that
  /// runs `up`.
  /// @param conn The connection object for managing @p fd.
  /// @param up The layer operating on top of this transport.
  static worker_ptr make_client(connection conn, upper_layer_ptr up);

private:
  // -- constructors, destructors, and assignment operators --------------------

  transport(stream_socket fd, connection conn, upper_layer_ptr up);

  policy_impl policy_impl_;
};

} // namespace caf::net::ssl
