// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/fwd.hpp"
#include "caf/net/ssl/errc.hpp"
#include "caf/net/stream_socket.hpp"

#include "caf/byte_span.hpp"
#include "caf/detail/net_export.hpp"

#include <cstddef>

namespace caf::net::ssl {

/// SSL state for a single connections.
class CAF_NET_EXPORT connection {
public:
  // -- member types -----------------------------------------------------------

  /// The default transport for exchanging raw bytes over an SSL connection.
  using transport_type = transport;

  /// The opaque implementation type.
  struct impl;

  // -- constructors, destructors, and assignment operators --------------------

  connection() = delete;

  connection(connection&& other);

  connection& operator=(connection&& other);

  ~connection();

  // -- native handles ---------------------------------------------------------

  /// Reinterprets `native_handle` as the native implementation type and takes
  /// ownership of the handle.
  static connection from_native(void* native_handle);

  /// Retrieves the native handle from the connection.
  void* native_handle() const noexcept;

  // -- error handling ---------------------------------------------------------

  /// Returns the error code for a preceding call to `connect`, `accept`,
  /// `read`, `write` or `close`.
  errc last_error(ptrdiff_t ret) const;

  /// Returns a human-readable representation of the error for a preceding call
  /// to `connect`, `accept`, `read`, `write` or `close`.
  /// @param ret The (negative) result from the preceding call.
  std::string last_error_string(ptrdiff_t ret) const;

  // -- SSL hostname validation and SNI support -------------------------------

  /// Sets the SNI hostname for this connection before handshake.
  /// Must be called before connect().
  [[nodiscard]] bool sni_hostname(const char* hostname) noexcept;

  /// Reads the SNI hostname from the connection.
  [[nodiscard]] const char* sni_hostname() noexcept;

  /// Sets the SSL hostname used for certificate validation of this connection.
  /// Must be called before connect().
  [[nodiscard]] bool hostname(const char* hostname) noexcept;

  // -- connecting and teardown ------------------------------------------------

  /// Performs the client-side TLS/SSL handshake after connection to the server.
  [[nodiscard]] ptrdiff_t connect();

  /// Performs the server-side TLS/SSL handshake after accepting a connection
  /// from a client.
  [[nodiscard]] ptrdiff_t accept();

  /// Gracefully closes the SSL connection without closing the socket.
  ptrdiff_t close();

  // -- reading and writing ----------------------------------------------------

  /// Tries to fill @p buf with data from the managed socket.
  [[nodiscard]] ptrdiff_t read(byte_span buf);

  /// Tries to write bytes from @p buf to the managed socket.
  [[nodiscard]] ptrdiff_t write(const_byte_span buf);

  // -- properties -------------------------------------------------------------

  /// Returns the number of bytes that are currently buffered outside of the
  /// managed socket.
  size_t buffered() const noexcept;

  /// Returns the file descriptor for this connection.
  stream_socket fd() const noexcept;

  bool valid() const noexcept {
    return pimpl_ != nullptr;
  }

  explicit operator bool() const noexcept {
    return valid();
  }

private:
  constexpr explicit connection(impl* ptr) : pimpl_(ptr) {
    // nop
  }

  impl* pimpl_;
};

} // namespace caf::net::ssl

// -- free functions -----------------------------------------------------------

namespace caf::net {

/// Checks whether `conn` contains a valid socket.
inline bool valid(const ssl::connection& conn) {
  return conn.valid();
}

/// Tries to fill `buf` with data from the managed socket of `conn`.
inline ptrdiff_t read(ssl::connection& conn, byte_span buf) {
  return conn.read(buf);
}

/// Tries to write bytes from `buf` to the managed socket of `conn`.
inline ptrdiff_t write(ssl::connection& conn, const_byte_span buf) {
  return conn.write(buf);
}

/// Returns the socket ID of `conn`.
inline socket_id get_socket_id(const ssl::connection& conn) noexcept {
  return conn.fd().id;
}

} // namespace caf::net
