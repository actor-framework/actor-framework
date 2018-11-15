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

#include "caf/logger.hpp"

#include <cstdint>

#include "caf/io/network/pipe_reader.hpp"
#include "caf/io/network/default_multiplexer.hpp"

#ifdef CAF_WINDOWS
# include <winsock2.h>
#else
# include <unistd.h>
# include <sys/socket.h>
#endif

namespace caf {
namespace io {
namespace network {

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
# ifdef CAF_WINDOWS
    auto res = recv(fd(), reinterpret_cast<socket_recv_ptr>(&ptrval),
                    sizeof(ptrval), 0);
# else
    auto res = read(fd(), &ptrval, sizeof(ptrval));
# endif
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

} // namespace network
} // namespace io
} // namespace caf
