/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/io/network/interfaces.hpp"

#include "caf/config.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <algorithm>

#ifdef CAF_WINDOWS
# ifndef _WIN32_WINNT
#   define _WIN32_WINNT 0x0600
# endif
# include <iostream>
# include <winsock2.h>
# include <ws2tcpip.h>
# include <iphlpapi.h>
#else
# include <sys/socket.h>
# include <netinet/in.h>
# include <net/if.h>
# include <unistd.h>
# include <netdb.h>
# include <ifaddrs.h>
# include <sys/ioctl.h>
# include <arpa/inet.h>
#endif

#include <memory>

#include "caf/detail/get_mac_addresses.hpp"

namespace caf {
namespace io {
namespace network {

// {interface_name => {protocol => address}}
using interfaces_map = std::map<std::string,
                                std::map<protocol,
                                         std::vector<std::string>>>;

template <class T>
void* vptr(T* ptr) {
  return static_cast<void*>(ptr);
}

void* fetch_in_addr(int family, sockaddr* addr) {
  if (family == AF_INET)
    return vptr(&reinterpret_cast<sockaddr_in*>(addr)->sin_addr);
  return vptr(&reinterpret_cast<sockaddr_in6*>(addr)->sin6_addr);
}

int fetch_addr_str(bool get_ipv4, bool get_ipv6,
                   char (&buf)[INET6_ADDRSTRLEN],
                   sockaddr* addr) {
  if (! addr)
    return AF_UNSPEC;
  auto family = addr->sa_family;
  auto in_addr = fetch_in_addr(family, addr);
  return ((family == AF_INET && get_ipv4) || (family == AF_INET6 && get_ipv6))
         && inet_ntop(family, in_addr, buf, INET6_ADDRSTRLEN) == buf
         ? family
         : AF_UNSPEC;
}

#ifdef CAF_WINDOWS

// F consumes `{interface_name, protocol, is_localhost, address}` entries.
template <class F>
void for_each_address(bool get_ipv4, bool get_ipv6, F fun) {
  ULONG tmp_size = 16 * 1024; // try 16kb buffer first
  IP_ADAPTER_ADDRESSES* tmp = nullptr;
  constexpr size_t max_tries = 3;
  size_t try_nr = 0;
  int retval = 0;
  do {
    if (tmp != nullptr)
      free(tmp);
    tmp = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(malloc(tmp_size));
    if (tmp == nullptr)
      throw std::bad_alloc();
    retval = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX,
                                  nullptr, tmp, &tmp_size);
  } while (retval == ERROR_BUFFER_OVERFLOW && ++try_nr < max_tries);
  std::unique_ptr<IP_ADAPTER_ADDRESSES, decltype(free)*> ifs{tmp, free};
  if (retval != NO_ERROR) {
    std::cerr << "Call to GetAdaptersAddresses failed with error: "
              << retval << std::endl;
    if (retval == ERROR_NO_DATA) {
      std::cerr << "No addresses were found for the requested parameters"
                << std::endl;
    } else {
      void* msgbuf = nullptr;
      if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER
                        | FORMAT_MESSAGE_FROM_SYSTEM
                        | FORMAT_MESSAGE_IGNORE_INSERTS,
                        nullptr, retval,
                        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                        (LPTSTR) &msgbuf, 0, nullptr)) {
        printf("Error: %s", static_cast<char*>(msgbuf));
        LocalFree(msgbuf);
      }
    }
    return;
  }
  char buffer[INET6_ADDRSTRLEN];
  for (auto i = ifs.get(); i != nullptr; i = i->Next) {
    for (auto j = i->FirstUnicastAddress; j != nullptr; j = j->Next) {
      auto addr = j->Address.lpSockaddr;
      auto family = fetch_addr_str(get_ipv4, get_ipv6, buffer, addr);
      if (family != AF_UNSPEC)
        fun(i->AdapterName, family == AF_INET ? protocol::ipv4 : protocol::ipv6,
            false, buffer);
    }
  }
}

#else // ifdef CAF_WINDOWS

// F consumes `{interface_name, protocol, is_localhost, address}` entries.
template <class F>
void for_each_address(bool get_ipv4, bool get_ipv6, F fun) {
  ifaddrs* tmp = nullptr;
  if (getifaddrs(&tmp) != 0) {
    perror("getifaddrs");
    return;
  }
  char buffer[INET6_ADDRSTRLEN];
  std::unique_ptr<ifaddrs, decltype(freeifaddrs)*> ifs{tmp, freeifaddrs};
  for (auto i = ifs.get(); i != nullptr; i = i->ifa_next) {
    auto family = fetch_addr_str(get_ipv4, get_ipv6, buffer, i->ifa_addr);
    if (family != AF_UNSPEC)
      fun(i->ifa_name, family == AF_INET ? protocol::ipv4 : protocol::ipv6,
          (i->ifa_flags & IFF_LOOPBACK) != 0,
          buffer);
  }
}

#endif // ifdef CAF_WINDOWS

namespace {

template <class F>
void traverse_impl(std::initializer_list<protocol> ps, F f) {
  if (std::find(ps.begin(), ps.end(), protocol::ethernet) != ps.end())
    for (auto& pair : detail::get_mac_addresses())
      f(pair.first.c_str(), protocol::ethernet, false, pair.second.c_str());
  auto get_ipv4 = std::find(ps.begin(), ps.end(), protocol::ipv4) != ps.end();
  auto get_ipv6 = std::find(ps.begin(), ps.end(), protocol::ipv6) != ps.end();
  for_each_address(get_ipv4, get_ipv6, f);
}

} // namespace <anonymous>

void interfaces::traverse(std::initializer_list<protocol> ps, consumer f) {
  traverse_impl(ps, f);
}

void interfaces::traverse(consumer f) {
  traverse_impl({protocol::ethernet, protocol::ipv4, protocol::ipv6}, f);
}

interfaces_map interfaces::list_all(bool include_localhost) {
  interfaces_map result;
  traverse_impl({protocol::ethernet, protocol::ipv4, protocol::ipv6},
                [&](const char* name, protocol p, bool lo, const char* addr) {
    if (include_localhost || ! lo)
      result[name][p].push_back(addr);
  });
  return result;
}

std::map<protocol, std::vector<std::string>>
interfaces::list_addresses(bool include_localhost) {
  std::map<protocol, std::vector<std::string>> result;
  traverse_impl({protocol::ethernet, protocol::ipv4, protocol::ipv6},
                [&](const char*, protocol p, bool lo, const char* addr) {
    if (include_localhost || ! lo)
      result[p].push_back(addr);
  });
  return result;
}

std::vector<std::string>
interfaces::list_addresses(std::initializer_list<protocol> procs,
                           bool include_localhost) {
  std::vector<std::string> result;
  traverse_impl(procs, [&](const char*, protocol, bool lo, const char* addr) {
    if (include_localhost || ! lo)
      result.push_back(addr);
  });
  return result;
}

std::vector<std::string> interfaces::list_addresses(protocol proc,
                                                    bool include_localhost) {
  return list_addresses({proc}, include_localhost);
}

maybe<std::pair<std::string, protocol>>
interfaces::native_address(const std::string& host,
                           maybe<protocol> preferred) {
  addrinfo hint;
  memset(&hint, 0, sizeof(hint));
  hint.ai_socktype = SOCK_STREAM;
  if (preferred)
    hint.ai_family = *preferred == protocol::ipv4 ? AF_INET : AF_INET6;
  addrinfo* tmp = nullptr;
  if (getaddrinfo(host.c_str(), nullptr, &hint, &tmp))
    return none;
  std::unique_ptr<addrinfo, decltype(freeaddrinfo)*> addrs{tmp, freeaddrinfo};
  char buffer[INET6_ADDRSTRLEN];
  for (auto i = addrs.get(); i != nullptr; i = i->ai_next) {
    auto family = fetch_addr_str(true, true, buffer, i->ai_addr);
    if (family != AF_UNSPEC)
      return {{buffer, family == AF_INET ? protocol::ipv4 : protocol::ipv6}};
  }
  return none;
}

} // namespace network
} // namespace io
} // namespace caf
