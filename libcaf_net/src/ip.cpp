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

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "caf/config.hpp"
#include "caf/detail/socket_sys_includes.hpp"
#include "caf/error.hpp"
#include "caf/ip_address.hpp"
#include "caf/ipv4_address.hpp"
#include "caf/logger.hpp"
#include "caf/string_algorithms.hpp"
#include "caf/string_view.hpp"

// clang-format off
#ifdef CAF_WINDOWS
#  ifndef _WIN32_WINNT
#    define _WIN32_WINNT 0x0600
#  endif
#  include <iostream>
#  include <iphlpapi.h>
#else
#  include <net/if.h>
#  include <netdb.h>
#  include <ifaddrs.h>
#  include <sys/ioctl.h>
#  include <sys/types.h>
#endif
// clang-format on

#ifndef HOST_NAME_MAX
#  define HOST_NAME_MAX 255
#endif

using std::pair;
using std::string;
using std::vector;

namespace caf {
namespace net {
namespace ip {

namespace {

constexpr auto v4_any_addr = "0.0.0.0";
constexpr auto v6_any_addr = "::";
constexpr auto localhost = "localhost";

template <class T>
void* vptr(T* ptr) {
  return static_cast<void*>(ptr);
}

void* fetch_in_addr(int family, sockaddr* addr) {
  if (family == AF_INET)
    return vptr(&reinterpret_cast<sockaddr_in*>(addr)->sin_addr);
  return vptr(&reinterpret_cast<sockaddr_in6*>(addr)->sin6_addr);
}

// TODO: Use getnameinfo instead?
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

void find_by_name(const vector<pair<string, ip_address>>& interfaces,
                  const string& name, vector<ip_address>& results) {
  for (auto& p : interfaces)
    if (p.first == name)
      results.push_back(p.second);
}

void find_by_addr(const vector<pair<string, ip_address>>& interfaces,
                  ip_address addr, vector<ip_address>& results) {
  for (auto& p : interfaces)
    if (p.second == addr)
      results.push_back(p.second);
}

} // namespace

std::vector<ip_address> resolve(const std::string& host) {
  addrinfo hint;
  memset(&hint, 0, sizeof(hint));
  hint.ai_socktype = SOCK_STREAM;
  hint.ai_family = AF_UNSPEC;
  if (host.empty())
    hint.ai_flags = AI_PASSIVE;
  addrinfo* tmp = nullptr;
  if (getaddrinfo(host.c_str(), nullptr, &hint, &tmp) != 0)
    return {};
  std::unique_ptr<addrinfo, decltype(freeaddrinfo)*> addrs{tmp, freeaddrinfo};
  char buffer[INET6_ADDRSTRLEN];
  std::vector<ip_address> results;
  for (auto i = addrs.get(); i != nullptr; i = i->ai_next) {
    auto family = fetch_addr_str(true, true, buffer, i->ai_addr);
    if (family != AF_UNSPEC) {
      ip_address ip;
      if (auto err = parse(buffer, ip)) {
        CAF_LOG_ERROR("could not parse into ip address " << buffer);
        continue;
      }
      results.emplace_back(ip);
    }
  }
  // TODO: Should we just prefer ipv6 or use a config option?
  // std::stable_sort(std::begin(results), std::end(results),
  //                  [](const ip_address& lhs, const ip_address& rhs) {
  //                    return !lhs.embeds_v4() && rhs.embeds_v4();
  //                  });
  return results;
}

std::vector<ip_address> local_addrs(const std::string& host) {
  std::vector<ip_address> results;
  // The string is not returned by getifaddrs, let's just do that ourselves.
  if (host == localhost)
    return {ip_address{{0}, {0x1}},
            ipv6_address{make_ipv4_address(127, 0, 0, 1)}};
  if (host == v4_any_addr || host == v6_any_addr)
    return {ip_address{}};
  ifaddrs* tmp = nullptr;
  if (getifaddrs(&tmp) != 0)
    return {};
  std::unique_ptr<ifaddrs, decltype(freeifaddrs)*> addrs{tmp, freeifaddrs};
  char buffer[INET6_ADDRSTRLEN];
  // Unless explicitly specified we are going to skip link-local addresses.
  auto is_link_local = starts_with(host, "fe80:");
  std::vector<std::pair<std::string, ip_address>> interfaces;
  for (auto i = addrs.get(); i != nullptr; i = i->ifa_next) {
    auto family = fetch_addr_str(true, true, buffer, i->ifa_addr);
    if (family != AF_UNSPEC) {
      ip_address ip;
      if (!is_link_local && starts_with(buffer, "fe80:")) {
        CAF_LOG_DEBUG("skipping link-local address: " << buffer);
        continue;
      } else if (auto err = parse(buffer, ip)) {
        CAF_LOG_ERROR("could not parse into ip address " << buffer);
        continue;
      }
      interfaces.emplace_back(std::string{i->ifa_name}, ip);
    }
  }
  ip_address host_addr;
  if (host.empty() || host == v4_any_addr || host == v6_any_addr)
    for (auto& p : interfaces)
      results.push_back(std::move(p.second));
  else if (auto err = parse(host, host_addr))
    find_by_name(interfaces, host, results);
  else
    find_by_addr(interfaces, host_addr, results);
  return results;
}

std::string hostname() {
  char buf[HOST_NAME_MAX + 1];
  buf[HOST_NAME_MAX] = '\0';
  gethostname(buf, HOST_NAME_MAX);
  gethostbyname(buf);
  return std::string{buf};
}

} // namespace ip
} // namespace net
} // namespace caf
