// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/udp_datagram_socket.hpp"

#include "caf/byte.hpp"
#include "caf/byte_buffer.hpp"
#include "caf/detail/convert_ip_endpoint.hpp"
#include "caf/detail/net_syscall.hpp"
#include "caf/detail/socket_sys_aliases.hpp"
#include "caf/detail/socket_sys_includes.hpp"
#include "caf/expected.hpp"
#include "caf/ip_endpoint.hpp"
#include "caf/logger.hpp"
#include "caf/net/socket_guard.hpp"
#include "caf/span.hpp"

namespace {

#if defined(CAF_WINDOWS) || defined(CAF_MACOS) || defined(CAF_IOS)             \
  || defined(CAF_BSD)
constexpr int no_sigpipe_io_flag = 0;
#else
constexpr int no_sigpipe_io_flag = MSG_NOSIGNAL;
#endif

} // namespace

namespace caf::net {

expected<udp_datagram_socket> make_udp_datagram_socket(ip_endpoint ep,
                                                       bool reuse_addr) {
  CAF_LOG_TRACE(CAF_ARG(ep));
  sockaddr_storage addr = {};
  detail::convert(ep, addr);
  CAF_NET_SYSCALL("socket", fd, ==, invalid_socket_id,
                  ::socket(addr.ss_family, SOCK_DGRAM, 0));
  udp_datagram_socket sock{fd};
  auto sguard = make_socket_guard(sock);
  socklen_t len = (addr.ss_family == AF_INET) ? sizeof(sockaddr_in)
                                              : sizeof(sockaddr_in6);
  if (reuse_addr) {
    int on = 1;
    CAF_NET_SYSCALL("setsockopt", tmp1, !=, 0,
                    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                               reinterpret_cast<setsockopt_ptr>(&on),
                               static_cast<socket_size_type>(sizeof(on))));
  }
  CAF_NET_SYSCALL("bind", err1, !=, 0,
                  ::bind(sock.id, reinterpret_cast<sockaddr*>(&addr), len));
  CAF_LOG_DEBUG(CAF_ARG(sock.id));
  return sguard.release();
}

ptrdiff_t read(udp_datagram_socket x, byte_span buf, ip_endpoint* src) {
  sockaddr_storage addr = {};
  socklen_t len = sizeof(sockaddr_storage);
  auto res = ::recvfrom(x.id, reinterpret_cast<socket_recv_ptr>(buf.data()),
                        buf.size(), no_sigpipe_io_flag,
                        reinterpret_cast<sockaddr*>(&addr), &len);
  if (src)
    std::ignore = detail::convert(addr, *src);
  return res;
}

ptrdiff_t write(udp_datagram_socket x, const_byte_span buf, ip_endpoint ep) {
  sockaddr_storage addr = {};
  detail::convert(ep, addr);
  auto len = ep.address().embeds_v4() ? sizeof(sockaddr_in)
                                      : sizeof(sockaddr_in6);
  return ::sendto(x.id, reinterpret_cast<socket_send_ptr>(buf.data()),
                  buf.size(), 0, reinterpret_cast<sockaddr*>(&addr),
                  static_cast<socklen_t>(len));
}

} // namespace caf::net
