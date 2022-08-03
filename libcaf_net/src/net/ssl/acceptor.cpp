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
