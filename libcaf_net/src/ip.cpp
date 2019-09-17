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

#ifdef CAF_WINDOWS
#  ifndef _WIN32_WINNT
#    define _WIN32_WINNT 0x0600
#  endif
#  include <iphlpapi.h>
#  include <winsock.h>
#else
#  include <ifaddrs.h>
#  include <net/if.h>
#  include <netdb.h>
#  include <sys/ioctl.h>
#endif

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

// Dummy port to resolve empty string with getaddrinfo.
constexpr string_view dummy_port = "42";
constexpr string_view v4_any_addr = "0.0.0.0";
constexpr string_view v6_any_addr = "::";
constexpr string_view localhost = "localhost";

void* fetch_in_addr(int family, sockaddr* addr) {
  if (family == AF_INET)
    return &reinterpret_cast<sockaddr_in*>(addr)->sin_addr;
  return &reinterpret_cast<sockaddr_in6*>(addr)->sin6_addr;
}

int fetch_addr_str(char (&buf)[INET6_ADDRSTRLEN], sockaddr* addr) {
  if (addr == nullptr)
    return AF_UNSPEC;
  auto family = addr->sa_family;
  auto in_addr = fetch_in_addr(family, addr);
  return (family == AF_INET || family == AF_INET6)
             && inet_ntop(family, in_addr, buf, INET6_ADDRSTRLEN) == buf
           ? family
           : AF_UNSPEC;
}

} // namespace

std::vector<ip_address> resolve(string_view host) {
  addrinfo hint;
  memset(&hint, 0, sizeof(hint));
  hint.ai_socktype = SOCK_STREAM;
  hint.ai_family = AF_UNSPEC;
  if (host.empty())
    hint.ai_flags = AI_PASSIVE;
  addrinfo* tmp = nullptr;
  std::string host_str{host.begin(), host.end()};
  if (getaddrinfo(host.empty() ? nullptr : host_str.c_str(),
                  host.empty() ? dummy_port.data() : nullptr, &hint, &tmp)
      != 0)
    return {};
  std::unique_ptr<addrinfo, decltype(freeaddrinfo)*> addrs{tmp, freeaddrinfo};
  char buffer[INET6_ADDRSTRLEN];
  std::vector<ip_address> results;
  for (auto i = addrs.get(); i != nullptr; i = i->ai_next) {
    auto family = fetch_addr_str(buffer, i->ai_addr);
    if (family != AF_UNSPEC) {
      ip_address ip;
      if (auto err = parse(buffer, ip))
        CAF_LOG_ERROR("could not parse IP address: " << buffer);
      else
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

std::string hostname() {
  char buf[HOST_NAME_MAX + 1];
  buf[HOST_NAME_MAX] = '\0';
  gethostname(buf, HOST_NAME_MAX);
  return buf;
}

#ifdef CAF_WINDOWS

template <class F>
void for_each_adapter(F f, bool is_link_local) {
  using adapters_ptr = std::unique_ptr<IP_ADAPTER_ADDRESSES, void (*)(void*)>;
  ULONG len = 0;
  if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr, nullptr,
                           &len)
      != ERROR_BUFFER_OVERFLOW) {
    CAF_LOG_ERROR("failed to get adapter addresses buffer length");
    return;
  }
  auto adapters = adapters_ptr{reinterpret_cast<IP_ADAPTER_ADDRESSES*>(
                                 ::malloc(len),
                               free};
  if (!adapters) {
    CAF_LOG_ERROR("malloc failed");
    return;
  }
  // TODO: The Microsoft WIN32 API example propopses to try three times, other
  // examples online just perform the call once. If we notice the call to be
  // unreliable, we might adapt that behavior.
  if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr,
                           adapters.get(), &len)
      != ERROR_SUCCESS) {
    CAF_LOG_ERROR("failed to get adapter addresses");
    return;
  }
  char ip_buf[INET6_ADDRSTRLEN];
  char name_buf[HOST_NAME_MAX];
  std::vector<std::pair<std::string, ip_address>> interfaces;
  for (auto* it = adapters.get(); it != nullptr; it = it->Next) {
    memset(name_buf, 0, HOST_NAME_MAX);
    WideCharToMultiByte(CP_ACP, 0, it->FriendlyName, wcslen(it->FriendlyName),
                        name_buf, HOST_NAME_MAX, nullptr, nullptr);
    std::string name{name_buf};
    for (auto* addr = it->FirstUnicastAddress; addr != nullptr;
         addr = addr->Next) {
      memset(ip_buf, 0, INET6_ADDRSTRLEN);
      getnameinfo(addr->Address.lpSockaddr, addr->Address.iSockaddrLength,
                  ip_buf, sizeof(ip_buf), nullptr, 0, NI_NUMERICHOST);
      ip_address ip;
      if (!is_link_local && starts_with(if_buf, "fe80:")) {
        CAF_LOG_DEBUG("skipping link-local address: " << ip_buf);
        continue;
      } else if (auto err = parse(ip_buf, ip))
        continue;
      f(name_buf, ip);
    }
  }
}

#else // CAF_WINDOWS

template <class F>
void for_each_adapter(F f, bool is_link_local) {
  ifaddrs* tmp = nullptr;
  if (getifaddrs(&tmp) != 0)
    return;
  std::unique_ptr<ifaddrs, decltype(freeifaddrs)*> addrs{tmp, freeifaddrs};
  char buffer[INET6_ADDRSTRLEN];
  std::vector<std::pair<std::string, ip_address>> interfaces;
  for (auto i = addrs.get(); i != nullptr; i = i->ifa_next) {
    auto family = fetch_addr_str(buffer, i->ifa_addr);
    if (family != AF_UNSPEC) {
      ip_address ip;
      if (!is_link_local && starts_with(buffer, "fe80:")) {
        CAF_LOG_DEBUG("skipping link-local address: " << buffer);
        continue;
      } else if (auto err = parse(buffer, ip)) {
        CAF_LOG_ERROR("could not parse into ip address " << buffer);
        continue;
      }
      f({i->ifa_name, strlen(i->ifa_name)}, ip);
    }
  }
}

#endif // CAF_WINDOWS

std::vector<ip_address> local_addresses(string_view host) {
  // TODO: If is any addr, call resolve with PR #23.
  if (host == v4_any_addr)
    return {ip_address{make_ipv4_address(0, 0, 0, 0)}};
  if (host == v6_any_addr)
    return {ip_address{}};
  // Unless explicitly specified we are going to skip link-local addresses.
  auto is_link_local = starts_with(host, "fe80:");
  std::vector<ip_address> results;
  ip_address host_addr;
  if (host.empty()) {
    for_each_adapter([&](string_view, ip_address ip) {
      results.push_back(ip);
    }, is_link_local);
  } else if (host == localhost) {
    auto v6_local = ip_address{{0}, {0x1}};
    auto v4_local = ip_address{make_ipv4_address(127, 0, 0, 1)};
    for_each_adapter([&](string_view, ip_address ip) {
      if (ip == v4_local || ip == v6_local)
        results.push_back(ip);
    }, is_link_local);
  } else if (auto err = parse(host, host_addr)) {
    for_each_adapter([&](string_view iface, ip_address ip) {
      if (iface == host)
        results.push_back(ip);
    }, is_link_local);
  } else {
    for_each_adapter([&](string_view, ip_address ip) {
      if (host_addr == ip)
        results.push_back(ip);
    }, is_link_local);
  }
  return results;
}

} // namespace ip
} // namespace net
} // namespace caf
