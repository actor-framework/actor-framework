/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include "caf/net/socket.hpp"

namespace caf {
namespace net {

/// Closes the guarded socket when destroyed.
template <class Socket>
class socket_guard {
public:
  explicit socket_guard(Socket fd) : fd_(fd) {
    // nop
  }

  ~socket_guard() {
    close();
  }

  Socket release() {
    auto fd = fd_;
    fd_ = Socket{};
    return fd;
  }

  void close() {
    if (fd_.id != net::invalid_socket) {
      CAF_LOG_DEBUG("close socket" << CAF_ARG(fd_));
      net::close(fd_);
      fd_ = Socket{};
    }
  }

private:
  Socket fd_;
};

template <class Socket>
socket_guard<Socket> make_socket_guard(Socket sock) {
  return socket_guard<Socket>{sock};
}

} // namespace net
} // namespace caf
