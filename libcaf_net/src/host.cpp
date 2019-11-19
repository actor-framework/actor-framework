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

#include "caf/net/host.hpp"

#include "caf/config.hpp"
#include "caf/detail/net_syscall.hpp"
#include "caf/detail/socket_sys_includes.hpp"
#include "caf/error.hpp"
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
