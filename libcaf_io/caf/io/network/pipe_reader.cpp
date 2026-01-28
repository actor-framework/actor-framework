// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/io/network/pipe_reader.hpp"

#include "caf/io/network/default_multiplexer.hpp"

#include "caf/byte_span.hpp"
#include "caf/logger.hpp"
#include "caf/net/pipe_socket.hpp"

#include <cstdint>

namespace caf::io::network {

pipe_reader::pipe_reader(default_multiplexer& dm)
  : event_handler(dm, net::invalid_socket_id) {
  // nop
}

void pipe_reader::removed_from_loop(operation) {
  // nop
}

void pipe_reader::graceful_shutdown() {
  net::shutdown_read(net::socket{fd_});
}

resumable* pipe_reader::try_read_next() {
  std::intptr_t ptrval;
  auto buf = byte_span{reinterpret_cast<std::byte*>(&ptrval), sizeof(ptrval)};
  auto res = net::read(net::pipe_socket{fd()}, buf);
  if (res != static_cast<ptrdiff_t>(sizeof(ptrval)))
    return nullptr;
  return reinterpret_cast<resumable*>(ptrval);
}

void pipe_reader::handle_event(operation op) {
  auto lg = log::io::trace("op = {}", op);
  if (op == operation::read) {
    auto ptr = try_read_next();
    if (ptr != nullptr)
      backend().resume({ptr, adopt_ref});
  }
  // else: ignore errors
}

void pipe_reader::init(net::socket_id sock_fd) {
  fd_ = sock_fd;
}

} // namespace caf::io::network
