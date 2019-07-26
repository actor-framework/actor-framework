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

#include "caf/net/datagram_socket.hpp"

#include "caf/detail/net_syscall.hpp"
#include "caf/detail/socket_sys_includes.hpp"
#include "caf/logger.hpp"

namespace caf {
namespace net {

#ifdef CAF_WINDOWS

error allow_connreset(datagram_socket x, bool new_value) {
  CAF_LOG_TRACE(CAF_ARG(x) << CAF_ARG(new_value));
  DWORD bytes_returned = 0;
  CAF_NET_SYSCALL("WSAIoctl", res, !=, 0,
                  WSAIoctl(x.id, _WSAIOW(IOC_VENDOR, 12), &new_value,
                           sizeof(new_value), NULL, 0, &bytes_returned, NULL,
                           NULL));
  return none;
}

#else // CAF_WINDOWS

error allow_connreset(datagram_socket, bool) {
  // nop; SIO_UDP_CONNRESET only exists on Windows
  return none;
}

#endif // CAF_WINDOWS

variant<size_t, sec>
check_datagram_socket_io_res(std::make_signed<size_t>::type res) {
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
