// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/internal/socket_sys_includes.hpp"

namespace caf::internal {

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

} // namespace caf::internal
