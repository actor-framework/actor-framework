// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/policy/udp.hpp"

#include "caf/log/io.hpp"
#include "caf/net/socket.hpp"

#include "caf/detail/socket_sys_aliases.hpp"

#ifdef CAF_WINDOWS
#  include <winsock2.h>
#else
#  include <sys/socket.h>
#  include <sys/types.h>
#endif

namespace caf::policy {

bool udp::read_datagram(size_t& result, net::socket_id fd, void* buf,
                        size_t buf_len, io::network::ip_endpoint& ep) {
  auto lg = log::io::trace("fd = {}", fd);
  memset(ep.address(), 0, sizeof(sockaddr_storage));
  net::socket_size_type len = sizeof(sockaddr_storage);
  auto sres = ::recvfrom(fd, reinterpret_cast<net::socket_recv_ptr>(buf),
                         buf_len, 0, ep.address(), &len);
  if (sres < 0 && !net::last_socket_error_is_temporary()) {
    log::io::error("recvfrom failed: {}",
                   net::last_socket_error_as_string());
    return false;
  }
  if (sres == 0)
    log::io::info("Received empty datagram");
  else if (sres > static_cast<ptrdiff_t>(buf_len))
    log::io::warning(
      "recvfrom cut of message, only received buf-len = {} of sres = {} bytes",
      buf_len, sres);
  result = (sres > 0) ? static_cast<size_t>(sres) : 0;
  *ep.length() = static_cast<size_t>(len);
  return true;
}

bool udp::write_datagram(size_t& result, net::socket_id fd, void* buf,
                         size_t buf_len, const io::network::ip_endpoint& ep) {
  auto lg = log::io::trace("fd = {}, buf_len = {}", fd, buf_len);
  net::socket_size_type len = static_cast<net::socket_size_type>(*ep.clength());
  auto sres = ::sendto(fd, reinterpret_cast<net::socket_send_ptr>(buf), buf_len,
                      0, ep.caddress(), len);
  if (sres < 0 && !net::last_socket_error_is_temporary()) {
    log::io::error("sendto failed: {}", net::last_socket_error_as_string());
    return false;
  }
  result = (sres > 0) ? static_cast<size_t>(sres) : 0;
  return true;
}

} // namespace caf::policy
