/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/net/udp_datagram_socket.hpp"

#include "caf/byte.hpp"
#include "caf/detail/convert_ip_endpoint.hpp"
#include "caf/detail/net_syscall.hpp"
#include "caf/detail/socket_sys_aliases.hpp"
#include "caf/detail/socket_sys_includes.hpp"
#include "caf/expected.hpp"
#include "caf/ip_endpoint.hpp"
#include "caf/logger.hpp"
#include "caf/net/socket_guard.hpp"
#include "caf/span.hpp"

namespace caf {
namespace net {

#ifdef CAF_WINDOWS

error allow_connreset(udp_datagram_socket x, bool new_value) {
  CAF_LOG_TRACE(CAF_ARG(x) << CAF_ARG(new_value));
  DWORD bytes_returned = 0;
  CAF_NET_SYSCALL("WSAIoctl", res, !=, 0,
                  WSAIoctl(x.id, _WSAIOW(IOC_VENDOR, 12), &new_value,
                           sizeof(new_value), NULL, 0, &bytes_returned, NULL,
                           NULL));
  return none;
}

#else // CAF_WINDOWS

error allow_connreset(udp_datagram_socket x, bool) {
  if (socket_cast<net::socket>(x) == invalid_socket)
    return sec::socket_invalid;
  // nop; SIO_UDP_CONNRESET only exists on Windows
  return none;
}

#endif // CAF_WINDOWS

expected<std::pair<udp_datagram_socket, uint16_t>>
make_udp_datagram_socket(ip_endpoint ep, bool reuse_addr) {
  CAF_LOG_TRACE(CAF_ARG(ep));
  sockaddr_storage addr;
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
  CAF_NET_SYSCALL("bind", err, !=, 0,
                  ::bind(sock.id, reinterpret_cast<sockaddr*>(&addr), len));
  CAF_NET_SYSCALL("getsockname", erro, !=, 0,
                  getsockname(sock.id, reinterpret_cast<sockaddr*>(&addr),
                              &len));
  CAF_LOG_DEBUG(CAF_ARG(sock.id));
  auto port = addr.ss_family == AF_INET
                ? reinterpret_cast<sockaddr_in*>(&addr)->sin_port
                : reinterpret_cast<sockaddr_in6*>(&addr)->sin6_port;
  return std::make_pair(sguard.release(), port);
}

variant<std::pair<size_t, ip_endpoint>, sec> read(udp_datagram_socket x,
                                                  span<byte> buf) {
  sockaddr_storage addr = {};
  socklen_t len = sizeof(sockaddr_storage);
  auto res = ::recvfrom(x.id, reinterpret_cast<socket_recv_ptr>(buf.data()),
                        buf.size(), 0, reinterpret_cast<sockaddr*>(&addr),
                        &len);
  auto ret = check_udp_datagram_socket_io_res(res);
  if (auto num_bytes = get_if<size_t>(&ret)) {
    CAF_LOG_INFO_IF(*num_bytes == 0, "Received empty datagram");
    CAF_LOG_WARNING_IF(*num_bytes > buf.size(),
                       "recvfrom cut of message, only received "
                         << CAF_ARG(buf.size()) << " of " << CAF_ARG(num_bytes)
                         << " bytes");
    ip_endpoint ep;
    if (auto err = detail::convert(addr, ep))
      return static_cast<sec>(err.code());
    return std::pair<size_t, ip_endpoint>(*num_bytes, ep);
  } else {
    return get<sec>(ret);
  }
}

variant<size_t, sec> write(udp_datagram_socket x, span<const byte> buf,
                           ip_endpoint ep) {
  sockaddr_storage addr;
  detail::convert(ep, addr);
  auto res = ::sendto(x.id, reinterpret_cast<socket_send_ptr>(buf.data()),
                      buf.size(), 0, reinterpret_cast<sockaddr*>(&addr),
                      ep.address().embeds_v4() ? sizeof(sockaddr_in)
                                               : sizeof(sockaddr_in6));
  auto ret = check_udp_datagram_socket_io_res(res);
  if (auto num_bytes = get_if<size_t>(&ret))
    return *num_bytes;
  else
    return get<sec>(ret);
}

variant<size_t, sec>
check_udp_datagram_socket_io_res(std::make_signed<size_t>::type res) {
  if (res < 0) {
    auto code = last_socket_error();
    if (code == std::errc::operation_would_block
        || code == std::errc::resource_unavailable_try_again)
      return sec::unavailable_or_would_block;
    return sec::socket_operation_failed;
  }
  return static_cast<size_t>(res);
}

} // namespace net
} // namespace caf
