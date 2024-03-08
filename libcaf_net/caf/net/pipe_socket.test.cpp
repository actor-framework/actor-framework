// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/pipe_socket.hpp"

#include "caf/test/test.hpp"

#include "caf/byte_buffer.hpp"

using namespace caf;
using namespace caf::net;

namespace {

TEST("send and receive") {
  byte_buffer send_buf{std::byte(1), std::byte(2), std::byte(3), std::byte(4),
                       std::byte(5), std::byte(6), std::byte(7), std::byte(8)};
  byte_buffer receive_buf;
  receive_buf.resize(100);
  pipe_socket rd_sock;
  pipe_socket wr_sock;
  auto maybe_sockets = make_pipe();
  require(maybe_sockets.has_value());
  std::tie(rd_sock, wr_sock) = *maybe_sockets;
  check_eq(static_cast<size_t>(write(wr_sock, send_buf)), send_buf.size());
  check_eq(static_cast<size_t>(read(rd_sock, receive_buf)), send_buf.size());
  check(std::equal(send_buf.begin(), send_buf.end(), receive_buf.begin()));
}

} // namespace
