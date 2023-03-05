// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/net_export.hpp"
#include "caf/expected.hpp"
#include "caf/net/ssl/context.hpp"
#include "caf/net/ssl/fwd.hpp"
#include "caf/net/tcp_accept_socket.hpp"

#include <variant>

namespace caf::detail {

/// Like @ref net::ssl::acceptor but with a `shared_ptr` to the context.
class CAF_NET_EXPORT shared_ssl_acceptor {
public:
  // -- member types -----------------------------------------------------------

  using transport_type = net::ssl::transport;

  // -- constructors, destructors, and assignment operators --------------------

  shared_ssl_acceptor() = delete;

  shared_ssl_acceptor(const shared_ssl_acceptor&) = default;

  shared_ssl_acceptor& operator=(const shared_ssl_acceptor&) = default;

  shared_ssl_acceptor(shared_ssl_acceptor&& other);

  shared_ssl_acceptor& operator=(shared_ssl_acceptor&& other);

  shared_ssl_acceptor(net::tcp_accept_socket fd,
                      std::shared_ptr<net::ssl::context> ctx)
    : fd_(fd), ctx_(std::move(ctx)) {
    // nop
  }

  // -- properties -------------------------------------------------------------

  net::tcp_accept_socket fd() const noexcept {
    return fd_;
  }

  net::ssl::context& ctx() noexcept {
    return *ctx_;
  }

  const net::ssl::context& ctx() const noexcept {
    return *ctx_;
  }

private:
  net::tcp_accept_socket fd_;
  std::shared_ptr<net::ssl::context> ctx_;
};

// -- free functions -----------------------------------------------------------

/// Checks whether `acc` has a valid socket descriptor.
bool CAF_NET_EXPORT valid(const shared_ssl_acceptor& acc);

/// Closes the socket of `obj`.
void CAF_NET_EXPORT close(shared_ssl_acceptor& acc);

/// Tries to accept a new connection on `acc`. On success, wraps the new socket
/// into an SSL @ref connection and returns it.
expected<net::ssl::connection> CAF_NET_EXPORT accept(shared_ssl_acceptor& acc);

} // namespace caf::detail
