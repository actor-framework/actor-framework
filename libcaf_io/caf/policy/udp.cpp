// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/policy/udp.hpp"

#include "caf/io/network/native_socket.hpp"

#include "caf/log/io.hpp"

#ifdef CAF_WINDOWS
#  include <winsock2.h>
#else
#  include <sys/socket.h>
#  include <sys/types.h>
#endif

using caf::io::network::is_error;
using caf::io::network::last_socket_error;
using caf::io::network::native_socket;
using caf::io::network::signed_size_type;
using caf::io::network::socket_error_as_string;
using caf::io::network::socket_size_type;

namespace caf::policy {

bool udp::read_datagram(size_t& result, native_socket fd, void* buf,
                        size_t buf_len, io::network::ip_endpoint& ep) {
  auto exit_guard = log::io::trace("fd = {}", fd);
  memset(ep.address(), 0, sizeof(sockaddr_storage));
  socket_size_type len = sizeof(sockaddr_storage);
  auto sres = ::recvfrom(fd, static_cast<io::network::socket_recv_ptr>(buf),
                         buf_len, 0, ep.address(), &len);
  if (is_error(sres, true)) {
    // Make sure WSAGetLastError gets called immediately on Windows.
    auto err = last_socket_error();
    CAF_IGNORE_UNUSED(err);
    log::io::error("recvfrom failed: {}", socket_error_as_string(err));
    return false;
  }
  if (sres == 0)
    log::io::info("Received empty datagram");
  else if (sres > static_cast<signed_size_type>(buf_len))
    log::io::warning(
      "recvfrom cut of message, only received buf-len = {} of sres = {} bytes",
      buf_len, sres);
  result = (sres > 0) ? static_cast<size_t>(sres) : 0;
  *ep.length() = static_cast<size_t>(len);
  return true;
}

bool udp::write_datagram(size_t& result, native_socket fd, void* buf,
                         size_t buf_len, const io::network::ip_endpoint& ep) {
  auto exit_guard = log::io::trace("fd = {}, buf_len = {}", fd, buf_len);
  socket_size_type len = static_cast<socket_size_type>(*ep.clength());
  auto sres = ::sendto(fd, reinterpret_cast<io::network::socket_send_ptr>(buf),
                       buf_len, 0, ep.caddress(), len);
  if (is_error(sres, true)) {
    // Make sure WSAGetLastError gets called immediately on Windows.
    auto err = last_socket_error();
    CAF_IGNORE_UNUSED(err);
    log::io::error("sendto failed: {}", socket_error_as_string(err));
    return false;
  }
  result = (sres > 0) ? static_cast<size_t>(sres) : 0;
  return true;
}

} // namespace caf::policy
