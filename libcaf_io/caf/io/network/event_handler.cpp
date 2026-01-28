// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/io/network/event_handler.hpp"

#include "caf/io/network/default_multiplexer.hpp"

#include "caf/log/io.hpp"
#include "caf/net/network_socket.hpp"
#include "caf/net/socket.hpp"
#include "caf/net/stream_socket.hpp"

namespace caf::io::network {

event_handler::event_handler(default_multiplexer& dm, net::socket_id sockfd)
  : fd_(sockfd),
    state_{true, false, false, false,
           to_integer(receive_policy_flag::at_least)},
    eventbf_(0),
    backend_(dm) {
  set_fd_flags();
}

event_handler::~event_handler() {
  if (fd_ != net::invalid_socket_id) {
    log::io::debug("close socket fd = {}", fd_);
    net::close(net::socket{fd_});
  }
}

void event_handler::passivate() {
  backend().del(operation::read, fd(), this);
}

void event_handler::activate() {
  backend().add(operation::read, fd(), this);
}

void event_handler::set_fd_flags() {
  if (fd_ == net::invalid_socket_id)
    return;
  // enable nonblocking IO, disable Nagle's algorithm, and suppress SIGPIPE
  net::nonblocking(net::socket{fd_}, true);
  net::nodelay(net::stream_socket{fd_}, true);
  net::allow_sigpipe(net::network_socket{fd_}, false);
}

} // namespace caf::io::network
