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

#include "caf/net/interfaces.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>

#ifdef CAF_WINDOWS
#  ifndef _WIN32_WINNT
#    define _WIN32_WINNT 0x0600
#  endif
#  include <iostream>
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <iphlpapi.h>
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <net/if.h>
#  include <unistd.h>
#  include <netdb.h>
#  include <ifaddrs.h>
#  include <sys/ioctl.h>
#  include <arpa/inet.h>
#endif

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "caf/detail/socket_sys_includes.hpp"

namespace caf {
namespace net {

namespace {

// {interface_name => {protocol => address}}
using interfaces_map = std::map<std::string,
                                std::map<ip, std::vector<std::string>>>;

template <class T>
void* vptr(T* ptr) {
  return static_cast<void*>(ptr);
}

void* fetch_in_addr(int family, sockaddr* addr) {
  if (family == AF_INET)
    return vptr(&reinterpret_cast<sockaddr_in*>(addr)->sin_addr);
  return vptr(&reinterpret_cast<sockaddr_in6*>(addr)->sin6_addr);
}

int fetch_addr_str(bool get_ipv4, bool get_ipv6, char (&buf)[INET6_ADDRSTRLEN],
                   sockaddr* addr) {
  if (addr == nullptr)
    return AF_UNSPEC;
  auto family = addr->sa_family;
  auto in_addr = fetch_in_addr(family, addr);
  return ((family == AF_INET && get_ipv4) || (family == AF_INET6 && get_ipv6))
             && inet_ntop(family, in_addr, buf, INET6_ADDRSTRLEN) == buf
           ? family
           : AF_UNSPEC;
}

} // namespace

optional<std::pair<std::string, ip>>
interfaces::native_address(const std::string& host, optional<ip> preferred) {
  addrinfo hint;
  memset(&hint, 0, sizeof(hint));
  hint.ai_socktype = SOCK_STREAM;
  if (preferred)
    hint.ai_family = *preferred == ip::v4 ? AF_INET : AF_INET6;
  addrinfo* tmp = nullptr;
  if (getaddrinfo(host.c_str(), nullptr, &hint, &tmp) != 0)
    return none;
  std::unique_ptr<addrinfo, decltype(freeaddrinfo)*> addrs{tmp, freeaddrinfo};
  char buffer[INET6_ADDRSTRLEN];
  for (auto i = addrs.get(); i != nullptr; i = i->ai_next) {
    auto family = fetch_addr_str(true, true, buffer, i->ai_addr);
    if (family != AF_UNSPEC)
      return std::make_pair(buffer, family == AF_INET ? ip::v4 : ip::v6);
  }
  return none;
}

std::vector<std::pair<std::string, ip>>
interfaces::server_address(uint16_t port, const char* host,
                           optional<ip> preferred) {
  using addr_pair = std::pair<std::string, ip>;
  addrinfo hint;
  memset(&hint, 0, sizeof(hint));
  hint.ai_socktype = SOCK_STREAM;
  if (preferred)
    hint.ai_family = *preferred == ip::v4 ? AF_INET : AF_INET6;
  else
    hint.ai_family = AF_UNSPEC;
  if (host == nullptr)
    hint.ai_flags = AI_PASSIVE;
  auto port_str = std::to_string(port);
  addrinfo* tmp = nullptr;
  if (getaddrinfo(host, port_str.c_str(), &hint, &tmp) != 0)
    return {};
  std::unique_ptr<addrinfo, decltype(freeaddrinfo)*> addrs{tmp, freeaddrinfo};
  char buffer[INET6_ADDRSTRLEN];
  // Take the first ipv6 address or the first available address otherwise
  std::vector<addr_pair> results;
  for (auto i = addrs.get(); i != nullptr; i = i->ai_next) {
    auto family = fetch_addr_str(true, true, buffer, i->ai_addr);
    if (family != AF_UNSPEC) {
      results.emplace_back(std::string{buffer},
                           family == AF_INET ? ip::v4 : ip::v6);
    }
  }
  std::stable_sort(std::begin(results), std::end(results),
                   [](const addr_pair& lhs, const addr_pair& rhs) {
                     return lhs.second > rhs.second;
                   });
  return results;
}

} // namespace net
} // namespace caf

