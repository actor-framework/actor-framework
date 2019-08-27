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

variant<std::pair<size_t, ip_endpoint>, sec> read(udp_datagram_socket x,
                                                  span<byte> buf) {
  sockaddr_in6 addr = {};
  socklen_t len = sizeof(sockaddr_in);
  auto res = ::recvfrom(x.id, reinterpret_cast<socket_recv_ptr>(buf.data()),
                        buf.size(), 0, reinterpret_cast<sockaddr*>(&addr),
                        &len);
  auto ret = check_udp_datagram_socket_io_res(res);
  if (auto num_bytes = get_if<size_t>(&ret)) {
    if (*num_bytes == 0)
      CAF_LOG_INFO("Received empty datagram");
    else if (*num_bytes > buf.size())
      CAF_LOG_WARNING("recvfrom cut of message, only received "
                      << CAF_ARG(buf.size()) << " of " << CAF_ARG(num_bytes)
                      << " bytes");
    auto ep = detail::to_ip_endpoint(addr);
    return std::pair<size_t, ip_endpoint>(*num_bytes, ep);
  } else {
    return get<sec>(ret);
  }
}

variant<size_t, sec> write(udp_datagram_socket x, span<const byte> buf,
                           ip_endpoint ep) {
  auto addr = detail::to_sockaddr(ep);
  auto res = ::sendto(x.id, reinterpret_cast<socket_send_ptr>(buf.data()),
                      buf.size(), 0, reinterpret_cast<sockaddr*>(&addr),
                      sizeof(sockaddr_in6));
  auto ret = check_udp_datagram_socket_io_res(res);
  if (auto num_bytes = get_if<size_t>(&ret))
    return *num_bytes;
  else
    return get<sec>(ret);
}

expected<uint16_t> bind(udp_datagram_socket x, ip_endpoint ep) {
  auto addr = to_sockaddr(ep);
  CAF_NET_SYSCALL("bind", err, !=, 0,
                  ::bind(x.id, reinterpret_cast<sockaddr*>(&addr),
                         sizeof(sockaddr_in6)));
  socklen_t len = sizeof(sockaddr_in6);
  CAF_NET_SYSCALL("getsockname", erro, !=, 0,
                  getsockname(x.id, reinterpret_cast<sockaddr*>(&addr), &len));
  return addr.sin6_port;
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
