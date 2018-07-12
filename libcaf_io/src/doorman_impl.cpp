/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/io/network/doorman_impl.hpp"

#include <algorithm>

#include "caf/logger.hpp"

#include "caf/io/network/default_multiplexer.hpp"

namespace caf {
namespace io {
namespace network {

doorman_impl::doorman_impl(default_multiplexer& mx, native_socket sockfd)
    : doorman(network::accept_hdl_from_socket(sockfd)),
      acceptor_(mx, sockfd) {
  // nop
}

bool doorman_impl::new_connection() {
  CAF_LOG_TRACE("");
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

void doorman_impl::stop_reading() {
  CAF_LOG_TRACE("");
  acceptor_.stop_reading();
  detach(&acceptor_.backend(), false);
}

void doorman_impl::launch() {
  CAF_LOG_TRACE("");
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

} // namespace network
} // namespace io
} // namespace caf
