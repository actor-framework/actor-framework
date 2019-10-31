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

#include "caf/net/socket_id.hpp"

namespace caf {
namespace net {

/// Closes the guarded socket when destroyed.
template <class Socket>
class socket_guard {
public:
  socket_guard() noexcept : sock_(invalid_socket_id) {
    // nop
  }

  explicit socket_guard(Socket sock) noexcept : sock_(sock) {
    // nop
  }

  socket_guard(socket_guard&& other) noexcept : sock_(other.release()) {
    // nop
  }

  socket_guard(const socket_guard&) = delete;

  socket_guard& operator=(socket_guard&& other) noexcept {
    reset(other.release());
    return *this;
  }

  socket_guard& operator=(const socket_guard&) = delete;

  ~socket_guard() {
    if (sock_.id != invalid_socket_id)
      close(sock_);
  }

  void reset(Socket x) noexcept {
    if (sock_.id != invalid_socket_id)
      close(sock_);
    sock_ = x;
  }

  Socket release() noexcept {
    auto sock = sock_;
    sock_.id = invalid_socket_id;
    return sock;
  }

  Socket socket() const noexcept {
    return sock_;
  }

private:
  Socket sock_;
};

template <class Socket>
socket_guard<Socket> make_socket_guard(Socket sock) {
  return socket_guard<Socket>{sock};
}

} // namespace net
} // namespace caf
