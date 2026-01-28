// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/io/network/acceptor.hpp"

#include "caf/detail/assert.hpp"
#include "caf/log/io.hpp"
#include "caf/net/socket.hpp"

namespace caf::io::network {

acceptor::acceptor(default_multiplexer& backend_ref, net::socket_id sockfd)
  : event_handler(backend_ref, sockfd), sock_(net::invalid_socket_id) {
  // nop
}

void acceptor::start(acceptor_manager* mgr) {
  auto lg = log::io::trace("fd = {}", fd_);
  CAF_ASSERT(mgr != nullptr);
  activate(mgr);
}

void acceptor::activate(acceptor_manager* mgr) {
  if (!mgr_) {
    mgr_.reset(mgr, add_ref);
    event_handler::activate();
  }
}

void acceptor::removed_from_loop(operation op) {
  auto lg = log::io::trace("fd = {}, op = {}", fd_, op);
  if (op == operation::read)
    mgr_.reset();
}

void acceptor::graceful_shutdown() {
  auto lg = log::io::trace("fd = {}", fd_);
  // Ignore repeated calls.
  if (state_.shutting_down)
    return;
  state_.shutting_down = true;
  // Shutdown socket activity.
  net::shutdown_read(net::socket{fd_});
  net::shutdown_write(net::socket{fd_});
}

} // namespace caf::io::network
