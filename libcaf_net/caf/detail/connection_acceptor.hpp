// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/fwd.hpp"

#include "caf/fwd.hpp"

#include <memory>

namespace caf::detail {

/// Accepts incoming connections and creates a socket manager for each. This
/// interface hides two implementation details: the actual acceptor (which
/// depends on the transport) and the protocol stack used for the accepted
/// connections.
class CAF_NET_EXPORT connection_acceptor {
public:
  virtual ~connection_acceptor();

  /// Callback from the socket manager for startup.
  virtual error start(net::socket_manager*) = 0;

  /// Aborts the acceptor.
  virtual void abort(const error&) = 0;

  /// Tries to accept a new connection.
  virtual expected<net::socket_manager_ptr> try_accept() = 0;

  /// Returns the socket handle of the acceptor.
  virtual net::socket handle() const = 0;
};

using connection_acceptor_ptr = std::unique_ptr<connection_acceptor>;

} // namespace caf::detail
