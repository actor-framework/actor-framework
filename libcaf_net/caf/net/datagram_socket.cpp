// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/datagram_socket.hpp"

#include "caf/detail/net_syscall.hpp"
#include "caf/detail/socket_sys_includes.hpp"
#include "caf/log/net.hpp"

#include <variant>

namespace caf::net {

#ifdef CAF_WINDOWS

error allow_connreset(datagram_socket x, bool new_value) {
  auto exit_guard = log::net::trace("x = {}, new_value = {}", x, new_value);
  DWORD bytes_returned = 0;
  CAF_NET_SYSCALL("WSAIoctl", res, !=, 0,
                  WSAIoctl(x.id, _WSAIOW(IOC_VENDOR, 12), &new_value,
                           sizeof(new_value), NULL, 0, &bytes_returned, NULL,
                           NULL));
  return none;
}

#else // CAF_WINDOWS

error allow_connreset(datagram_socket x, bool) {
  if (x == invalid_socket)
    return sec::socket_invalid;
  // nop; SIO_UDP_CONNRESET only exists on Windows
  return none;
}

#endif // CAF_WINDOWS

std::variant<size_t, sec>
check_datagram_socket_io_res(std::make_signed_t<size_t> res) {
  if (res < 0) {
    auto code = last_socket_error();
    if (code == std::errc::operation_would_block
        || code == std::errc::resource_unavailable_try_again)
      return sec::unavailable_or_would_block;
    return sec::socket_operation_failed;
  }
  return static_cast<size_t>(res);
}

} // namespace caf::net
