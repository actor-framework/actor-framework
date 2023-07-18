// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/io/network/pipe_reader.hpp"

#include "caf/io/network/default_multiplexer.hpp"

#include "caf/logger.hpp"

#include <cstdint>

#ifdef CAF_WINDOWS
#  include <winsock2.h>
#else
#  include <sys/socket.h>
#  include <unistd.h>
#endif

namespace caf::io::network {

pipe_reader::pipe_reader(default_multiplexer& dm)
  : event_handler(dm, invalid_native_socket) {
  // nop
}

void pipe_reader::removed_from_loop(operation) {
  // nop
}

void pipe_reader::graceful_shutdown() {
  shutdown_read(fd_);
}

resumable* pipe_reader::try_read_next() {
  std::intptr_t ptrval;
  // on windows, we actually have sockets, otherwise we have file handles
#ifdef CAF_WINDOWS
  auto res = recv(fd(), reinterpret_cast<socket_recv_ptr>(&ptrval),
                  sizeof(ptrval), 0);
#else
  auto res = read(fd(), &ptrval, sizeof(ptrval));
#endif
  if (res != sizeof(ptrval))
    return nullptr;
  return reinterpret_cast<resumable*>(ptrval);
}

void pipe_reader::handle_event(operation op) {
  CAF_LOG_TRACE(CAF_ARG(op));
  if (op == operation::read) {
    auto ptr = try_read_next();
    if (ptr != nullptr)
      backend().resume({ptr, false});
  }
  // else: ignore errors
}

void pipe_reader::init(native_socket sock_fd) {
  fd_ = sock_fd;
}

} // namespace caf::io::network
