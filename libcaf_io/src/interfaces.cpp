/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
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

#include <errno.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <net/if.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

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

template <class SockaddrType, int Family>
void add_addr(ifaddrs* first, std::vector<std::string>& res) {
  auto addr = reinterpret_cast<SockaddrType*>(first->ifa_addr);
  auto in_addr = fetch_in_addr(addr);
  char address_buffer[INET6_ADDRSTRLEN + 1];
  inet_ntop(Family, in_addr, address_buffer, INET6_ADDRSTRLEN);
  res.push_back(address_buffer);
}

template <class F>
void for_each_device(bool include_localhost, F fun) {
  char host[NI_MAXHOST];
  ifaddrs* tmp = nullptr;
  if (getifaddrs(&tmp) != 0) {
    perror("getifaddrs");
    return;
  }
  std::unique_ptr<ifaddrs, decltype(freeifaddrs)*> ifs{tmp, freeifaddrs};
  for (auto i = ifs.get(); i != nullptr; i = i->ifa_next) {
    auto family = i->ifa_addr->sa_family;
    if (include_localhost) {
      fun(i, family);
    } else if (family == AF_INET || family == AF_INET6) {
      auto ok = getnameinfo(i->ifa_addr,
                            family == AF_INET ? sizeof(sockaddr_in)
                                              : sizeof(sockaddr_in6),
                            host, NI_MAXHOST, nullptr, 0, 0);
      if (ok == 0 && strcmp("localhost", host) != 0) {
        fun(i, family);
      }
    }
  }
}

interfaces_map interfaces::list_all(bool include_localhost) {
  interfaces_map result;
  for (auto& pair : detail::get_mac_addresses()) {
    result[pair.first][protocol::ethernet].push_back(std::move(pair.second));
  }
  for_each_device(include_localhost, [&](ifaddrs* i, int family) {
    if (family == AF_INET) {
      add_addr<sockaddr_in, AF_INET>(i, result[i->ifa_name][protocol::ipv4]);
    } else if (family == AF_INET6) {
      add_addr<sockaddr_in6, AF_INET6>(i, result[i->ifa_name][protocol::ipv6]);
    }
  });
  return result;
}

std::map<protocol, std::vector<std::string>>
interfaces::list_addresses(bool include_localhost) {
  std::map<protocol, std::vector<std::string>> result;
  for (auto& pair : detail::get_mac_addresses()) {
    result[protocol::ethernet].push_back(std::move(pair.second));
  }
  for_each_device(include_localhost, [&](ifaddrs* i, int family) {
    if (family == AF_INET) {
      add_addr<sockaddr_in, AF_INET>(i, result[protocol::ipv4]);
    } else if (family == AF_INET6) {
      add_addr<sockaddr_in6, AF_INET6>(i, result[protocol::ipv6]);
    }
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
      for_each_device(include_localhost, [&](ifaddrs* i, int family) {
        if (family == AF_INET) {
          add_addr<sockaddr_in, AF_INET>(i, result);
        }
      });
      break;
    case protocol::ipv6:
      for_each_device(include_localhost, [&](ifaddrs* i, int family) {
        if (family == AF_INET6) {
          add_addr<sockaddr_in6, AF_INET6>(i, result);
        }
      });
      break;
  }
  return result;
}

} // namespace network
} // namespace io
} // namespace caf
