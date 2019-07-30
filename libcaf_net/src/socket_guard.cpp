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

#include "caf/net/socket_guard.hpp"

#include "caf/logger.hpp"

namespace caf {
namespace net {

socket_guard::socket_guard(net::socket fd) : fd_(fd) {
  // nop
}

socket_guard::~socket_guard() {
  close();
}

net::socket socket_guard::release() {
  auto fd = fd_;
  fd_ = net::invalid_socket;
  return fd;
}

void socket_guard::close() {
  if (fd_ != net::invalid_socket) {
    CAF_LOG_DEBUG("close socket" << CAF_ARG(fd_));
    net::close(fd_);
    fd_ = net::invalid_socket;
  }
}

} // namespace net
} // namespace caf
