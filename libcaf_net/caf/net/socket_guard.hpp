// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/socket_id.hpp"

namespace caf::net {

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

} // namespace caf::net
