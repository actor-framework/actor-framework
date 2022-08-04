// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/ssl/acceptor.hpp"

#include "caf/expected.hpp"
#include "caf/net/ssl/connection.hpp"
#include "caf/net/tcp_stream_socket.hpp"

namespace caf::net::ssl {

// -- constructors, destructors, and assignment operators ----------------------

acceptor::acceptor(acceptor&& other)
  : fd_(other.fd_), ctx_(std::move(other.ctx_)) {
  other.fd_.id = invalid_socket_id;
}

acceptor& acceptor::operator=(acceptor&& other) {
  fd_ = other.fd_;
  ctx_ = std::move(other.ctx_);
  other.fd_.id = invalid_socket_id;
  return *this;
}

// -- factories ----------------------------------------------------------------

expected<acceptor>
acceptor::make_with_cert_file(tcp_accept_socket fd, const char* cert_file_path,
                              const char* key_file_path, format file_format) {
  auto ctx = context::make_server(tls::any);
  if (!ctx) {
    return {make_error(sec::runtime_error, "unable to create SSL context")};
  }
  if (!ctx->use_certificate_from_file(cert_file_path, file_format)) {
    return {make_error(sec::runtime_error, "unable to load certificate file",
                       ctx->last_error_string())};
  }
  if (!ctx->use_private_key_from_file(key_file_path, file_format)) {
    return {make_error(sec::runtime_error, "unable to load private key file",
                       ctx->last_error_string())};
  }
  return {acceptor{fd, std::move(*ctx)}};
}

expected<acceptor>
acceptor::make_with_cert_file(uint16_t port, const char* cert_file_path,
                              const char* key_file_path, format file_format) {
  if (auto fd = make_tcp_accept_socket(port)) {
    return acceptor::make_with_cert_file(*fd, cert_file_path, key_file_path,
                                         file_format);
  } else {
    return {make_error(sec::cannot_open_port)};
  }
}

// -- free functions -----------------------------------------------------------

bool valid(const acceptor& acc) {
  return valid(acc.fd());
}

void close(acceptor& acc) {
  close(acc.fd());
}

expected<connection> accept(acceptor& acc) {
  if (auto fd = accept(acc.fd()); fd) {
    return acc.ctx().new_connection(*fd);
  } else {
    return expected<connection>{std::move(fd.error())};
  }
}

} // namespace caf::net::ssl
