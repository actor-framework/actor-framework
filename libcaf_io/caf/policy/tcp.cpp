// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/policy/tcp.hpp"

#include "caf/io/network/rw_state.hpp"

#include "caf/net/socket.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/net/tcp_accept_socket.hpp"
#include "caf/net/tcp_stream_socket.hpp"

#include "caf/byte_span.hpp"
#include "caf/expected.hpp"
#include "caf/log/io.hpp"

#include <cstring>

namespace caf::policy {

io::network::rw_state tcp::read_some(size_t& result, net::socket_id fd,
                                     void* buf, size_t len) {
  auto lg = log::io::trace("fd = {}, len = {}", fd, len);
  auto buf_span = byte_span{reinterpret_cast<std::byte*>(buf), len};
  auto sres = net::read(net::stream_socket{fd}, buf_span);
  if (sres < 0 && !net::last_socket_error_is_temporary()) {
    log::io::error("recv failed: {}", net::last_socket_error_as_string());
    return io::network::rw_state::failure;
  }
  if (sres == 0) {
    log::io::debug("peer performed orderly shutdown fd = {}", fd);
    return io::network::rw_state::failure;
  }
  log::io::debug("len = {} fd = {} sres = {}", len, fd, sres);
  result = (sres > 0) ? static_cast<size_t>(sres) : 0;
  return io::network::rw_state::success;
}

io::network::rw_state tcp::write_some(size_t& result, net::socket_id fd,
                                      const void* buf, size_t len) {
  auto lg = log::io::trace("fd = {}, len = {}", fd, len);
  auto buf_span = const_byte_span{reinterpret_cast<const std::byte*>(buf), len};
  auto sres = net::write(net::stream_socket{fd}, buf_span);
  if (sres < 0 && !net::last_socket_error_is_temporary()) {
    log::io::error("send failed: {}", net::last_socket_error_as_string());
    return io::network::rw_state::failure;
  }
  log::io::debug("len = {} fd = {} sres = {}", len, fd, sres);
  result = (sres > 0) ? static_cast<size_t>(sres) : 0;
  return io::network::rw_state::success;
}

bool tcp::try_accept(net::socket_id& result, net::socket_id fd) {
  auto lg = log::io::trace("fd = {}", fd);
  auto accepted = net::accept(net::tcp_accept_socket{fd});
  if (accepted) {
    if (auto err = net::child_process_inherit(*accepted, false); err.valid()) {
      net::close(*accepted);
      log::io::error("child process inherit failed: {}", err);
      return false;
    }
    result = accepted->id;
    log::io::debug("fd = {} result = {}", fd, result);
    return true;
  }
  log::io::error("accept on fd {} failed: {}", fd, accepted.error());
  return false;
}

} // namespace caf::policy
