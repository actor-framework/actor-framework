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
  explicit socket_guard(Socket sock) : sock_(sock) {
    // nop
  }

  ~socket_guard() {
    using net::close;
    if (sock_.id != invalid_socket_id) {
      close(sock_);
      sock_.id = invalid_socket_id;
    }
  }

  Socket release() {
    auto sock = sock_;
    sock_.id = invalid_socket_id;
    return sock;
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
