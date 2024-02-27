// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/io/network/doorman_impl.hpp"

#include "caf/io/network/default_multiplexer.hpp"

#include "caf/logger.hpp"

#include <algorithm>

namespace caf::io::network {

doorman_impl::doorman_impl(default_multiplexer& mx, native_socket sockfd)
  : doorman(network::accept_hdl_from_socket(sockfd)), acceptor_(mx, sockfd) {
  // nop
}

bool doorman_impl::new_connection() {
  auto exit_guard = log::io::trace("");
  if (detached())
    // we are already disconnected from the broker while the multiplexer
    // did not yet remove the socket, this can happen if an I/O event causes
    // the broker to call close_all() while the pollset contained
    // further activities for the broker
    return false;
  auto& dm = acceptor_.backend();
  auto sptr = dm.new_scribe(acceptor_.accepted_socket());
  auto hdl = sptr->hdl();
  parent()->add_scribe(std::move(sptr));
  return doorman::new_connection(&dm, hdl);
}

void doorman_impl::graceful_shutdown() {
  auto exit_guard = log::io::trace("");
  acceptor_.graceful_shutdown();
  detach(&acceptor_.backend(), false);
}

void doorman_impl::launch() {
  auto exit_guard = log::io::trace("");
  acceptor_.start(this);
}

std::string doorman_impl::addr() const {
  auto x = local_addr_of_fd(acceptor_.fd());
  if (!x)
    return "";
  return std::move(*x);
}

uint16_t doorman_impl::port() const {
  auto x = local_port_of_fd(acceptor_.fd());
  if (!x)
    return 0;
  return *x;
}

void doorman_impl::add_to_loop() {
  acceptor_.activate(this);
}

void doorman_impl::remove_from_loop() {
  acceptor_.passivate();
}

} // namespace caf::io::network
