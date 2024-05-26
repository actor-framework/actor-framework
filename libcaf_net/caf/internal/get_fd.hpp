// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/socket.hpp"
#include "caf/net/ssl/connection.hpp"
#include "caf/net/ssl/tcp_acceptor.hpp"

#include <memory>

namespace caf::internal {

inline auto get_fd(net::socket fd) {
  return fd;
}

inline auto get_fd(const net::ssl::tcp_acceptor& acceptor) {
  return acceptor.fd();
}

inline auto get_fd(const net::ssl::connection& conn) {
  return conn.fd();
}

} // namespace caf::internal
