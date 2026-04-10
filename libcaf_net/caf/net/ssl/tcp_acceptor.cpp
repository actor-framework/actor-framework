// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/ssl/tcp_acceptor.hpp"

#include "caf/net/ssl/connection.hpp"
#include "caf/net/tcp_stream_socket.hpp"

#include "caf/expected.hpp"
#include "caf/format_to_error.hpp"
#include "caf/format_to_unexpected.hpp"

namespace caf::net::ssl {

// -- constructors, destructors, and assignment operators ----------------------

tcp_acceptor::tcp_acceptor(tcp_acceptor&& other)
  : fd_(other.fd_), ctx_(std::move(other.ctx_)) {
  other.fd_.id = invalid_socket_id;
}

tcp_acceptor& tcp_acceptor::operator=(tcp_acceptor&& other) {
  fd_ = other.fd_;
  ctx_ = std::move(other.ctx_);
  other.fd_.id = invalid_socket_id;
  return *this;
}

// -- factories ----------------------------------------------------------------

expected<tcp_acceptor>
tcp_acceptor::make_with_cert_file(tcp_accept_socket fd,
                                  const char* cert_file_path,
                                  const char* key_file_path,
                                  format file_format) {
  auto ssl_ctx = context::make_server(tls::any);
  if (!ssl_ctx) {
    return expected<tcp_acceptor>{unexpect, sec::runtime_error,
                                  "unable to create SSL context"};
  }
  if (!ssl_ctx->use_certificate_file(cert_file_path, file_format)) {
    return format_to_unexpected(sec::runtime_error,
                                "unable to load certificate file: {}",
                                ssl_ctx->last_error_string());
  }
  if (!ssl_ctx->use_private_key_file(key_file_path, file_format)) {
    return format_to_unexpected(sec::runtime_error,
                                "unable to load private key file: {}",
                                ssl_ctx->last_error_string());
  }
  return {tcp_acceptor{fd, std::move(*ssl_ctx)}};
}

expected<tcp_acceptor>
tcp_acceptor::make_with_cert_file(uint16_t port, const char* cert_file_path,
                                  const char* key_file_path,
                                  format file_format) {
  if (auto accept_fd = make_tcp_accept_socket(port)) {
    return tcp_acceptor::make_with_cert_file(*accept_fd, cert_file_path,
                                             key_file_path, file_format);
  } else {
    return expected<tcp_acceptor>{unexpect, sec::cannot_open_port};
  }
}

// -- free functions -----------------------------------------------------------

bool valid(const tcp_acceptor& acc) {
  return valid(acc.fd());
}

void close(tcp_acceptor& acc) {
  close(static_cast<socket>(acc.fd()));
}

expected<connection> accept(tcp_acceptor& acc) {
  auto fd = accept(acc.fd());
  if (fd)
    return acc.ctx().new_connection(*fd);
  return expected<connection>{unexpect, std::move(fd.error())};
}

} // namespace caf::net::ssl
