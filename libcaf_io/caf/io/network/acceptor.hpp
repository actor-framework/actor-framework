// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/io/fwd.hpp"
#include "caf/io/network/acceptor_manager.hpp"
#include "caf/io/network/event_handler.hpp"
#include "caf/io/network/operation.hpp"
#include "caf/net/socket_id.hpp"

#include "caf/detail/io_export.hpp"
#include "caf/log/io.hpp"
#include "caf/ref_counted.hpp"

namespace caf::io::network {

/// An acceptor is responsible for accepting incoming connections.
class CAF_IO_EXPORT acceptor : public event_handler {
public:
  /// A manager providing the `accept` member function.
  using manager_type = acceptor_manager;

  /// A smart pointer to an acceptor manager.
  using manager_ptr = intrusive_ptr<manager_type>;

  acceptor(default_multiplexer& backend_ref, net::socket_id sockfd);

  /// Returns the accepted socket. This member function should
  /// be called only from the `new_connection` callback.
  net::socket_id& accepted_socket() {
    return sock_;
  }

  /// Starts this acceptor, forwarding all incoming connections to
  /// `manager`. The intrusive pointer will be released after the
  /// acceptor has been closed or an IO error occurred.
  void start(acceptor_manager* mgr);

  /// Activates the acceptor.
  void activate(acceptor_manager* mgr);

  void removed_from_loop(operation op) override;

  void graceful_shutdown() override;

protected:
  template <class Policy>
  void handle_event_impl(io::network::operation op, Policy& policy) {
    auto lg = log::io::trace("fd = {}, op = {}", fd(), op);
    if (mgr_ && op == operation::read) {
      net::socket_id sockfd = net::invalid_socket_id;
      if (policy.try_accept(sockfd, fd())) {
        if (sockfd != net::invalid_socket_id) {
          sock_ = sockfd;
          mgr_->new_connection();
        }
      }
    }
  }

private:
  manager_ptr mgr_;
  net::socket_id sock_;
};

} // namespace caf::io::network
