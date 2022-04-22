// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/stream_transport.hpp"

namespace caf::net::openssl {

ptrdiff_t policy::read(stream_socket, byte_span buf) {
  return SSL_read(conn_.get(), buf.data(), static_cast<int>(buf.size()));
}

ptrdiff_t policy::write(stream_socket, const_byte_span buf) {
  return SSL_write(conn_.get(), buf.data(), static_cast<int>(buf.size()));
}

stream_transport_error policy::last_error(stream_socket fd, ptrdiff_t ret) {
  switch (SSL_get_error(conn_.get(), static_cast<int>(ret))) {
    case SSL_ERROR_NONE:
    case SSL_ERROR_WANT_ACCEPT:
    case SSL_ERROR_WANT_CONNECT:
      // For all of these, OpenSSL docs say to do the operation again later.
      return stream_transport_error::temporary;
    case SSL_ERROR_SYSCALL:
      // Need to consult errno, which we just leave to the default policy.
      return default_stream_transport_policy::last_error(fd, ret);
    case SSL_ERROR_WANT_READ:
      return stream_transport_error::want_read;
    case SSL_ERROR_WANT_WRITE:
      return stream_transport_error::want_write;
    default:
      // Errors like SSL_ERROR_WANT_X509_LOOKUP are technically temporary, but
      // we do not configure any callbacks. So seeing this is a red flag.
      return stream_transport_error::permanent;
  }
}

ptrdiff_t stream_transport::default_policy::connect(stream_socket x) {
  return SSL_connect(conn_.get());
}

ptrdiff_t stream_transport::default_policy::accept(stream_socket) {
  return SSL_accept(conn_.get());
}

size_t stream_transport::default_policy::buffered() const noexcept {
  return static_cast<size_t>(SSL_pending(conn_.get()));
}

} // namespace caf::net::openssl

namespace caf::net {

openssl_transport::openssl_transport(stream_socket fd, openssl::conn_ptr conn,
                                     upper_layer_ptr up)
  : super(fd, std::move(up), &ssl_policy_), ssl_policy_(std::move(conn)) {
  // nop
}

} // namespace caf::net
