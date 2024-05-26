// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/io/network/event_handler.hpp"

#include "caf/io/network/default_multiplexer.hpp"

#include "caf/log/io.hpp"

#ifdef CAF_WINDOWS
#  include <winsock2.h>
#else
#  include <sys/socket.h>
#endif

namespace caf::io::network {

event_handler::event_handler(default_multiplexer& dm, native_socket sockfd)
  : fd_(sockfd),
    state_{true, false, false, false,
           to_integer(receive_policy_flag::at_least)},
    eventbf_(0),
    backend_(dm) {
  set_fd_flags();
}

event_handler::~event_handler() {
  if (fd_ != invalid_native_socket) {
    log::io::debug("close socket fd = {}", fd_);
    close_socket(fd_);
  }
}

void event_handler::passivate() {
  backend().del(operation::read, fd(), this);
}

void event_handler::activate() {
  backend().add(operation::read, fd(), this);
}

void event_handler::set_fd_flags() {
  if (fd_ == invalid_native_socket)
    return;
  // enable nonblocking IO, disable Nagle's algorithm, and suppress SIGPIPE
  nonblocking(fd_, true);
  tcp_nodelay(fd_, true);
  allow_sigpipe(fd_, false);
}

} // namespace caf::io::network
