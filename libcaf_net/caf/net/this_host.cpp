// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/this_host.hpp"

#include "caf/net/socket.hpp"

#include "caf/config.hpp"
#include "caf/error.hpp"
#include "caf/internal/net_syscall.hpp"
#include "caf/internal/socket_sys_includes.hpp"
#include "caf/message.hpp"
#include "caf/none.hpp"

namespace caf::net {

#ifdef CAF_WINDOWS

void this_host::startup() {
  WSADATA WinsockData;
  CAF_NET_CRITICAL_SYSCALL("WSAStartup", result, !=, 0,
                           WSAStartup(MAKEWORD(2, 2), &WinsockData));
}

void this_host::cleanup() {
  WSACleanup();
}

#else // CAF_WINDOWS

void this_host::startup() {
  // nop
}

void this_host::cleanup() {
  // nop
}

#endif // CAF_WINDOWS

} // namespace caf::net
