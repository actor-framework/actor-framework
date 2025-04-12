// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/socket_id.hpp"

#include "caf/detail/net_export.hpp"

namespace caf::net {

/// Closes the guarded socket when destroyed.
template <class Socket>
class CAF_NET_EXPORT socket_guard {
public:
  socket_guard() noexcept : fd_(invalid_socket_id) {
    // nop
  }

  explicit socket_guard(Socket fd) noexcept : fd_(fd) {
    // nop
  }

  socket_guard(socket_guard&& other) noexcept : fd_(other.release()) {
    // nop
  }

  socket_guard(const socket_guard&) = delete;

  socket_guard& operator=(socket_guard&& other) noexcept {
    reset(other.release());
    return *this;
  }

  socket_guard& operator=(const socket_guard&) = delete;

  ~socket_guard() {
    if (fd_.id != invalid_socket_id)
      close(fd_);
  }

  void reset(Socket x) noexcept {
    if (fd_.id != invalid_socket_id)
      close(fd_);
    fd_ = x;
  }

  void reset() noexcept {
    if (fd_.id != invalid_socket_id) {
      close(fd_);
      fd_.id = invalid_socket_id;
    }
  }

  Socket release() noexcept {
    auto sock = fd_;
    fd_.id = invalid_socket_id;
    return sock;
  }

  Socket get() noexcept {
    return fd_;
  }

  Socket socket() const noexcept {
    return fd_;
  }

private:
  Socket fd_;
};

template <class Socket>
socket_guard<Socket> make_socket_guard(Socket sock) {
  return socket_guard<Socket>{sock};
}

} // namespace caf::net
