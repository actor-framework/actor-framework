/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include "caf/detail/socket_sys_includes.hpp"

namespace caf::detail {

inline auto addr_of(sockaddr_in& what) -> decltype(what.sin_addr)& {
  return what.sin_addr;
}

inline auto family_of(sockaddr_in& what) -> decltype(what.sin_family)& {
  return what.sin_family;
}

inline auto port_of(sockaddr_in& what) -> decltype(what.sin_port)& {
  return what.sin_port;
}

inline auto addr_of(sockaddr_in6& what) -> decltype(what.sin6_addr)& {
  return what.sin6_addr;
}

inline auto family_of(sockaddr_in6& what) -> decltype(what.sin6_family)& {
  return what.sin6_family;
}

inline auto port_of(sockaddr_in6& what) -> decltype(what.sin6_port)& {
  return what.sin6_port;
}

} // namespace caf
