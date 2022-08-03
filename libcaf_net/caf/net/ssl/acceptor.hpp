// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/net_export.hpp"
#include "caf/net/ssl/context.hpp"
#include "caf/net/ssl/fwd.hpp"
#include "caf/net/tcp_accept_socket.hpp"

namespace caf::net::ssl {

/// Wraps an accept socket and an SSL context.
class CAF_NET_EXPORT acceptor {
public:
  // -- constructors, destructors, and assignment operators --------------------

  acceptor() = delete;

  acceptor(const acceptor&) = delete;

  acceptor& operator=(const acceptor&) = delete;

  acceptor(acceptor&& other);

  acceptor& operator=(acceptor&& other);

  acceptor(tcp_accept_socket fd, context ctx) : fd_(fd), ctx_(std::move(ctx)) {
    // nop
  }

  // -- properties -------------------------------------------------------------

  tcp_accept_socket fd() const noexcept {
    return fd_;
  }

  context& ctx() noexcept {
    return ctx_;
  }

  const context& ctx() const noexcept {
    return ctx_;
  }

private:
  tcp_accept_socket fd_;
  context ctx_;
};

/// Checks whether `acc` has a valid socket descriptor.
bool CAF_NET_EXPORT valid(const acceptor& acc);

/// Closes the socket of `obj`.
void CAF_NET_EXPORT close(acceptor& acc);

/// Tries to accept a new connection on `acc`. On success, wraps the new socket
/// into an SSL @ref connection and returns it.
expected<connection> CAF_NET_EXPORT accept(acceptor& acc);

} // namespace caf::net::ssl
