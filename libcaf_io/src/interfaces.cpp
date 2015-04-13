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

#ifdef CAF_WINDOWS
# ifndef _WIN32_WINNT
#   define _WIN32_WINNT 0x0600
# endif
# include <iostream>
# include <winsock2.h>
# include <ws2tcpip.h>
# include <iphlpapi.h>
# pragma comment(lib, "ws2_32.lib")
# pragma comment(lib, "iphlpapi.lib")
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

in6_addr* fetch_in_addr(sockaddr_in6* addr) {
  return &(addr->sin6_addr);
}

in_addr* fetch_in_addr(sockaddr_in* addr) {
  return &(addr->sin_addr);
}

template <int Family, class SockAddr>
void add_addr_as_string(std::vector<std::string>& res, SockAddr* addr) {
  auto in_addr = fetch_in_addr(addr);
  char address_buffer[INET6_ADDRSTRLEN + 1];
  inet_ntop(Family, in_addr, address_buffer, INET6_ADDRSTRLEN);
  res.push_back(address_buffer);
}

#ifdef CAF_WINDOWS

using if_device_ptr = IP_ADAPTER_ADDRESSES*;

const char* if_device_name(if_device_ptr ptr) {
  return ptr->AdapterName;
}

template <int Family>
void add_addr(if_device_ptr ptr, std::vector<std::string>& res) {
  static_assert(Family == AF_INET || Family == AF_INET6,
                "invalid address family");
  using addr_type =
    typename std::conditional<
      Family == AF_INET,
      sockaddr_in*,
      sockaddr_in6*
    >::type;
  for (auto i = ptr->FirstUnicastAddress; i != nullptr; i = i->Next) {
    if (i->Address.lpSockaddr->sa_family == Family) {
      auto addr = reinterpret_cast<addr_type>(i->Address.lpSockaddr);
      add_addr_as_string<Family>(res, addr);
    }
  }
}

template <class F>
void for_each_device(bool include_localhost, F fun) {
  ULONG tmp_size = 16 * 1024; // try 16kb buffer first
  IP_ADAPTER_ADDRESSES* tmp = nullptr;
  constexpr size_t max_tries = 3;
  size_t try_nr = 0;
  int retval = 0;
  do {
    if (tmp != nullptr) {
      free(tmp);
    }
    tmp = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(malloc(tmp_size));
    if (tmp == nullptr) {
      throw std::bad_alloc();
    }
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
        printf("Error: %s", msgbuf);
        LocalFree(msgbuf);
      }
    }
    return;
  }
  for (auto i = ifs.get(); i != nullptr; i = i->Next) {
    fun(i);
  }
}

#else // ifdef CAF_WINDOWS

// interface address pointer
using if_device_ptr = ifaddrs*;

const char* if_device_name(if_device_ptr ptr) {
  return ptr->ifa_name;
}

template <int Family>
void add_addr(if_device_ptr ptr, std::vector<std::string>& res) {
  static_assert(Family == AF_INET || Family == AF_INET6,
                "invalid address family");
  using addr_type =
    typename std::conditional<
      Family == AF_INET,
      sockaddr_in*,
      sockaddr_in6*
    >::type;
  if (ptr->ifa_addr->sa_family != Family) {
    return;
  }
  add_addr_as_string<Family>(res, reinterpret_cast<addr_type>(ptr->ifa_addr));
}

template <class F>
void for_each_device(bool include_localhost, F fun) {
  char host[NI_MAXHOST];
  if_device_ptr tmp = nullptr;
  if (getifaddrs(&tmp) != 0) {
    perror("getifaddrs");
    return;
  }
  std::unique_ptr<ifaddrs, decltype(freeifaddrs)*> ifs{tmp, freeifaddrs};
  for (auto i = ifs.get(); i != nullptr; i = i->ifa_next) {
    auto family = i->ifa_addr->sa_family;
    if (include_localhost) {
      fun(i);
    } else if (family == AF_INET || family == AF_INET6) {
      auto len = static_cast<socklen_t>(family == AF_INET
                                        ? sizeof(sockaddr_in)
                                        : sizeof(sockaddr_in6));
      auto ok = getnameinfo(i->ifa_addr, len, host, NI_MAXHOST, nullptr, 0, 0);
      if (ok == 0 && strcmp("localhost", host) != 0) {
        fun(i);
      }
    }
  }
}

#endif // ifdef CAF_WINDOWS

interfaces_map interfaces::list_all(bool include_localhost) {
  interfaces_map result;
  for (auto& pair : detail::get_mac_addresses()) {
    result[pair.first][protocol::ethernet].push_back(std::move(pair.second));
  }
  for_each_device(include_localhost, [&](if_device_ptr i) {
    add_addr<AF_INET>(i, result[if_device_name(i)][protocol::ipv4]);
    add_addr<AF_INET6>(i, result[if_device_name(i)][protocol::ipv6]);
  });
  return result;
}

std::map<protocol, std::vector<std::string>>
interfaces::list_addresses(bool include_localhost) {
  std::map<protocol, std::vector<std::string>> result;
  for (auto& pair : detail::get_mac_addresses()) {
    result[protocol::ethernet].push_back(std::move(pair.second));
  }
  for_each_device(include_localhost, [&](if_device_ptr i) {
    add_addr<AF_INET>(i, result[protocol::ipv4]);
    add_addr<AF_INET6>(i, result[protocol::ipv6]);
  });
  return result;
}

std::vector<std::string> interfaces::list_addresses(protocol proc,
                                                    bool include_localhost) {
  std::vector<std::string> result;
  switch (proc) {
    case protocol::ethernet:
      for (auto& pair : detail::get_mac_addresses()) {
        result.push_back(std::move(pair.second));
      }
      break;
    case protocol::ipv4:
      for_each_device(include_localhost, [&](if_device_ptr i) {
        add_addr<AF_INET>(i, result);
      });
      break;
    case protocol::ipv6:
      for_each_device(include_localhost, [&](if_device_ptr i) {
        add_addr<AF_INET6>(i, result);
      });
      break;
  }
  return result;
}

optional<std::pair<std::string, protocol>>
interfaces::native_address(const std::string& host,
                           optional<protocol> preferred) {
  addrinfo hint;
  memset(&hint, 0, sizeof(hint));
  hint.ai_socktype = SOCK_STREAM;
  if (preferred) {
    hint.ai_family = *preferred == protocol::ipv4 ? AF_INET : AF_INET6;
  }
  addrinfo* tmp = nullptr;
  if (getaddrinfo(host.c_str(), nullptr, &hint, &tmp)) {
    return none;
  }
  std::unique_ptr<addrinfo, decltype(freeaddrinfo)*> addrs{tmp, freeaddrinfo};
  for (auto i = addrs.get(); i != nullptr; i = i->ai_next) {
    auto family = i->ai_family;
    if (family == AF_INET || family == AF_INET6) {
      char buffer[INET6_ADDRSTRLEN];
      auto res = family == AF_INET
           ? inet_ntop(family,
                       &reinterpret_cast<sockaddr_in*>(i->ai_addr)->sin_addr,
                       buffer, sizeof(buffer))
           : inet_ntop(family,
                       &reinterpret_cast<sockaddr_in6*>(i->ai_addr)->sin6_addr,
                       buffer, sizeof(buffer));
      if (res != nullptr) {
        return {{res, family == AF_INET ? protocol::ipv4 : protocol::ipv6}};
      }
    }
  }
  return none;
}

} // namespace network
} // namespace io
} // namespace caf
