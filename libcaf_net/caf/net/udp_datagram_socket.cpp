// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/udp_datagram_socket.hpp"

#include "caf/net/socket_guard.hpp"

#include "caf/byte_buffer.hpp"
#include "caf/detail/socket_sys_aliases.hpp"
#include "caf/expected.hpp"
#include "caf/internal/net_syscall.hpp"
#include "caf/internal/socket_sys_includes.hpp"
#include "caf/ip_endpoint.hpp"
#include "caf/log/net.hpp"
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

namespace {

void convert(const ip_endpoint& src, sockaddr_storage& dst) {
  memset(&dst, 0, sizeof(sockaddr_storage));
  if (src.address().embeds_v4()) {
    auto sockaddr4 = reinterpret_cast<sockaddr_in*>(&dst);
    sockaddr4->sin_family = AF_INET;
    sockaddr4->sin_port = ntohs(src.port());
    sockaddr4->sin_addr.s_addr = src.address().embedded_v4().bits();
  } else {
    auto sockaddr6 = reinterpret_cast<sockaddr_in6*>(&dst);
    sockaddr6->sin6_family = AF_INET6;
    sockaddr6->sin6_port = ntohs(src.port());
    memcpy(&sockaddr6->sin6_addr, src.address().bytes().data(),
           src.address().bytes().size());
  }
}

error convert(const sockaddr_storage& src, ip_endpoint& dst) {
  if (src.ss_family == AF_INET) {
    auto sockaddr4 = reinterpret_cast<const sockaddr_in&>(src);
    ipv4_address ipv4_addr;
    memcpy(ipv4_addr.data().data(), &sockaddr4.sin_addr, ipv4_addr.size());
    dst = ip_endpoint{ipv4_addr, htons(sockaddr4.sin_port)};
  } else if (src.ss_family == AF_INET6) {
    auto sockaddr6 = reinterpret_cast<const sockaddr_in6&>(src);
    ipv6_address ipv6_addr;
    memcpy(ipv6_addr.bytes().data(), &sockaddr6.sin6_addr,
           ipv6_addr.bytes().size());
    dst = ip_endpoint{ipv6_addr, htons(sockaddr6.sin6_port)};
  } else {
    return sec::invalid_argument;
  }
  return none;
}

} // namespace

expected<udp_datagram_socket> make_udp_datagram_socket(ip_endpoint ep,
                                                       bool reuse_addr) {
  auto lg = log::net::trace("ep = {}", ep);
  sockaddr_storage addr = {};
  convert(ep, addr);
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
  log::net::debug("sock.id = {}", sock.id);
  return sguard.release();
}

ptrdiff_t read(udp_datagram_socket x, byte_span buf, ip_endpoint* src) {
  sockaddr_storage addr = {};
  socklen_t len = sizeof(sockaddr_storage);
  auto res = ::recvfrom(x.id, reinterpret_cast<socket_recv_ptr>(buf.data()),
                        buf.size(), no_sigpipe_io_flag,
                        reinterpret_cast<sockaddr*>(&addr), &len);
  if (src)
    std::ignore = convert(addr, *src);
  return res;
}

ptrdiff_t write(udp_datagram_socket x, const_byte_span buf, ip_endpoint ep) {
  sockaddr_storage addr = {};
  convert(ep, addr);
  auto len = ep.address().embeds_v4() ? sizeof(sockaddr_in)
                                      : sizeof(sockaddr_in6);
  return ::sendto(x.id, reinterpret_cast<socket_send_ptr>(buf.data()),
                  buf.size(), 0, reinterpret_cast<sockaddr*>(&addr),
                  static_cast<socklen_t>(len));
}

} // namespace caf::net
