// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/default_enum_inspect.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"
#include "caf/is_error_code_enum.hpp"

namespace caf::net::ssl {

/// SSL error code for I/O operations on a @ref connection.
enum class errc : uint8_t {
  /// Not-an-error.
  none = 0,
  /// SSL has closed the connection. The underlying transport may remain open.
  closed,
  /// Temporary error. SSL failed to write to a socket because it needs to read
  /// first.
  want_read,
  /// Temporary error. SSL failed to read from a socket because it needs to
  /// write first.
  want_write,
  /// Temporary error. The SSL client handshake did not complete yet.
  want_connect,
  /// Temporary error. The SSL server handshake did not complete yet.
  want_accept,
  /// Temporary error. An application callback has asked to be called again.
  want_x509_lookup,
  /// Temporary error. An asynchronous is still processing data and the user
  /// must call the preceding function again from the same thread.
  want_async,
  /// The pool for starting asynchronous jobs is exhausted.
  want_async_job,
  /// Temporary error. An application callback has asked to be called again.
  want_client_hello,
  /// The operating system reported a non-recoverable, fatal I/O error. Users
  /// may consult OS-specific means to retrieve the underlying error, e.g.,
  /// `errno` on UNIX or `WSAGetLastError` on Windows.
  syscall_failed,
  /// SSL encountered a fatal error, usually a protocol violation.
  fatal,
  /// An unexpected error occurred with no further explanation available.
  unspecified,
};

/// @relates errc
CAF_NET_EXPORT std::string to_string(errc);

/// @relates errc
CAF_NET_EXPORT bool from_string(std::string_view, errc&);

/// @relates errc
CAF_NET_EXPORT bool from_integer(std::underlying_type_t<errc>, errc&);

/// @relates errc
template <class Inspector>
bool inspect(Inspector& f, errc& x) {
  return default_enum_inspect(f, x);
}

} // namespace caf::net::ssl

namespace caf::detail {

CAF_NET_EXPORT net::ssl::errc ssl_errc_from_native(int);

} // namespace caf::detail

CAF_ERROR_CODE_ENUM(caf::net::ssl::errc)
