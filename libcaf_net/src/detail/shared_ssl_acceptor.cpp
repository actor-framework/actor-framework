// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/shared_ssl_acceptor.hpp"

#include "caf/expected.hpp"
#include "caf/net/ssl/connection.hpp"
#include "caf/net/tcp_stream_socket.hpp"

namespace caf::detail {

// -- constructors, destructors, and assignment operators ----------------------

shared_ssl_acceptor::shared_ssl_acceptor(shared_ssl_acceptor&& other)
  : fd_(other.fd_), ctx_(std::move(other.ctx_)) {
  other.fd_.id = net::invalid_socket_id;
}

shared_ssl_acceptor&
shared_ssl_acceptor::operator=(shared_ssl_acceptor&& other) {
  fd_ = other.fd_;
  ctx_ = std::move(other.ctx_);
  other.fd_.id = net::invalid_socket_id;
  return *this;
}

// -- free functions -----------------------------------------------------------

bool valid(const shared_ssl_acceptor& acc) {
  return valid(acc.fd());
}

void close(shared_ssl_acceptor& acc) {
  close(acc.fd());
}

expected<net::ssl::connection> accept(shared_ssl_acceptor& acc) {
  auto fd = accept(acc.fd());
  if (fd)
    return acc.ctx().new_connection(*fd);
  return expected<net::ssl::connection>{std::move(fd.error())};
}

} // namespace caf::detail
