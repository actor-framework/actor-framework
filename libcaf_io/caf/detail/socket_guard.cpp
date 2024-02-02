// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/socket_guard.hpp"

#ifdef CAF_WINDOWS
#  include <winsock2.h>
#else
#  include <unistd.h>
#endif

#include "caf/log/io.hpp"

namespace caf::detail {

socket_guard::socket_guard(io::network::native_socket fd) : fd_(fd) {
  // nop
}

socket_guard::~socket_guard() {
  close();
}

io::network::native_socket socket_guard::release() {
  auto fd = fd_;
  fd_ = io::network::invalid_native_socket;
  return fd;
}

void socket_guard::close() {
  if (fd_ != io::network::invalid_native_socket) {
    log::io::debug("close socket fd = {}", fd_);
    io::network::close_socket(fd_);
    fd_ = io::network::invalid_native_socket;
  }
}

} // namespace caf::detail
