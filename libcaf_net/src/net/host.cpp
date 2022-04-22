// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/host.hpp"

#include "caf/config.hpp"
#include "caf/detail/net_syscall.hpp"
#include "caf/detail/socket_sys_includes.hpp"
#include "caf/error.hpp"
#include "caf/message.hpp"
#include "caf/net/socket.hpp"
#include "caf/none.hpp"

namespace caf::net {

#ifdef CAF_WINDOWS

error this_host::startup() {
  WSADATA WinsockData;
  CAF_NET_SYSCALL("WSAStartup", result, !=, 0,
                  WSAStartup(MAKEWORD(2, 2), &WinsockData));
  return none;
}

void this_host::cleanup() {
  WSACleanup();
}

#else // CAF_WINDOWS

error this_host::startup() {
  return none;
}

void this_host::cleanup() {
  // nop
}

#endif // CAF_WINDOWS

} // namespace caf::net
