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

#include "caf/io/network/datagram_servant_impl.hpp"

#include <algorithm>

#include "caf/logger.hpp"

#include "caf/io/network/default_multiplexer.hpp"

namespace caf {
namespace io {
namespace network {

datagram_servant_impl::datagram_servant_impl(default_multiplexer& mx,
                                             native_socket sockfd, int64_t id)
    : datagram_servant(datagram_handle::from_int(id)),
      launched_(false),
      handler_(mx, sockfd) {
  // nop
}

bool datagram_servant_impl::new_endpoint(network::receive_buffer& buf) {
  CAF_LOG_TRACE("");
  if (detached())
     // We are already disconnected from the broker while the multiplexer
     // did not yet remove the socket, this can happen if an I/O event
     // causes the broker to call close_all() while the pollset contained
     // further activities for the broker.
     return false;
  // A datagram that has a source port of zero is valid and never requires a
  // reply. In the case of CAF we can simply drop it as nothing but the
  // handshake could be communicated which we could not reply to.
  // Source: TCP/IP Illustrated, Chapter 10.2
  if (network::port(handler_.sending_endpoint()) == 0)
    return true;
  auto& dm = handler_.backend();
  auto hdl = datagram_handle::from_int(dm.next_endpoint_id());
  add_endpoint(handler_.sending_endpoint(), hdl);
  parent()->add_hdl_for_datagram_servant(this, hdl);
  return consume(&dm, hdl, buf);
}

void datagram_servant_impl::ack_writes(bool enable) {
  CAF_LOG_TRACE(CAF_ARG(enable));
  handler_.ack_writes(enable);
}

std::vector<char>& datagram_servant_impl::wr_buf(datagram_handle hdl) {
  return handler_.wr_buf(hdl);
}

void datagram_servant_impl::enqueue_datagram(datagram_handle hdl,
                                             std::vector<char> buffer) {
  handler_.enqueue_datagram(hdl, std::move(buffer));
}

network::receive_buffer& datagram_servant_impl::rd_buf() {
  return handler_.rd_buf();
}

void datagram_servant_impl::stop_reading() {
  CAF_LOG_TRACE("");
  handler_.stop_reading();
  detach_handles();
  detach(&handler_.backend(), false);
}

void datagram_servant_impl::flush() {
  CAF_LOG_TRACE("");
  handler_.flush(this);
}

std::string datagram_servant_impl::addr() const {
  auto x = remote_addr_of_fd(handler_.fd());
  if (!x)
    return "";
  return *x;
}

uint16_t datagram_servant_impl::port(datagram_handle hdl) const {
  auto& eps = handler_.endpoints();
  auto itr = eps.find(hdl);
  if (itr == eps.end())
    return 0;
  return network::port(itr->second);
}

uint16_t datagram_servant_impl::local_port() const {
  auto x = local_port_of_fd(handler_.fd());
  if (!x)
    return 0;
  return *x;
}

std::vector<datagram_handle> datagram_servant_impl::hdls() const {
  std::vector<datagram_handle> result;
  result.reserve(handler_.endpoints().size());
  for (auto& p : handler_.endpoints())
    result.push_back(p.first);
  return result;
}

void datagram_servant_impl::add_endpoint(const ip_endpoint& ep,
                                         datagram_handle hdl) {
  handler_.add_endpoint(hdl, ep, this);
}

void datagram_servant_impl::remove_endpoint(datagram_handle hdl) {
  handler_.remove_endpoint(hdl);
}

void datagram_servant_impl::launch() {
  CAF_LOG_TRACE("");
  CAF_ASSERT(!launched_);
  launched_ = true;
  handler_.start(this);
}

void datagram_servant_impl::add_to_loop() {
  handler_.activate(this);
}

void datagram_servant_impl::remove_from_loop() {
  handler_.passivate();
}


void datagram_servant_impl::detach_handles() {
  for (auto& p : handler_.endpoints()) {
    if (p.first != hdl())
      parent()->erase(p.first);
  }
}

} // namespace network
} // namespace io
} // namespace caf
