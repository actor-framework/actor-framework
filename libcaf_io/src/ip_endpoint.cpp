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

#include "caf/io/network/ip_endpoint.hpp"

#include "caf/sec.hpp"
#include "caf/logger.hpp"

#include "caf/io/network/native_socket.hpp"

#ifdef CAF_WINDOWS
# include <winsock2.h>
# include <windows.h>
# include <ws2tcpip.h>
# include <ws2ipdef.h>
#else
# include <unistd.h>
# include <cerrno>
# include <arpa/inet.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <netinet/ip.h>
#endif

#ifdef CAF_WINDOWS
using sa_family_t = short;
#endif

namespace {

template <class SizeType = size_t>
struct hash_conf {
  template <class T = SizeType>
  static constexpr caf::detail::enable_if_t<(sizeof(T) == 4), size_t> basis() {
    return 2166136261u;
  }
  template <class T = SizeType>
  static constexpr caf::detail::enable_if_t<(sizeof(T) == 4), size_t> prime() {
    return 16777619u;
  }
  template <class T = SizeType>
  static constexpr caf::detail::enable_if_t<(sizeof(T) == 8), size_t> basis() {
    return 14695981039346656037u;
  }
  template <class T = SizeType>
  static constexpr caf::detail::enable_if_t<(sizeof(T) == 8), size_t> prime() {
    return 1099511628211u;
  }
};

constexpr uint8_t static_bytes[] = {
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xFF, 0xFF
};

constexpr size_t prehash(int i = 11) {
  return (i > 0)
    ? (prehash(i - 1) * hash_conf<>::prime()) ^ static_bytes[i]
    : (hash_conf<>::basis() * hash_conf<>::prime()) ^ static_bytes[i];
}

} // namespace <anonymous>

namespace caf {
namespace io {
namespace network {

struct ip_endpoint::impl {
  sockaddr_storage addr;
  size_t len;
};

ip_endpoint::ip_endpoint() : ptr_(new ip_endpoint::impl) {
  // nop
}

ip_endpoint::ip_endpoint(const ip_endpoint& other) {
  ptr_.reset(new ip_endpoint::impl);
  memcpy(address(), other.caddress(), sizeof(sockaddr_storage));
  *length() = *other.clength();
}

ip_endpoint& ip_endpoint::operator=(const ip_endpoint& other) {
  ptr_.reset(new ip_endpoint::impl);
  memcpy(address(), other.caddress(), sizeof(sockaddr_storage));
  *length() = *other.clength();
  return *this;
}

sockaddr* ip_endpoint::address() {
  return reinterpret_cast<struct sockaddr*>(&ptr_->addr);
}

const sockaddr* ip_endpoint::caddress() const {
  return reinterpret_cast<const struct sockaddr*>(&ptr_->addr);
}

size_t* ip_endpoint::length() {
  return &ptr_->len;
}

const size_t* ip_endpoint::clength() const {
  return &ptr_->len;
}

void ip_endpoint::clear() {
  memset(&ptr_->addr, 0, sizeof(sockaddr_storage));
  ptr_->len = 0;
}

void ip_endpoint::impl_deleter::operator()(ip_endpoint::impl *ptr) const {
  delete ptr;
}

ep_hash::ep_hash() {
  // nop
}

size_t ep_hash::operator()(const sockaddr& sa) const noexcept {
  switch (sa.sa_family) {
    case AF_INET:
      return hash(reinterpret_cast<const struct sockaddr_in*>(&sa));
    case AF_INET6:
      return hash(reinterpret_cast<const struct sockaddr_in6*>(&sa));
    default:
      CAF_LOG_ERROR("Only IPv4 and IPv6 are supported.");
      return 0;
  }
}

size_t ep_hash::hash(const sockaddr_in* sa) const noexcept {
  auto& addr = sa->sin_addr;
  size_t res = prehash();
  // the first loop was replaces with `constexpr size_t prehash()`
  for (int i = 0; i < 4; ++i) {
    res = res * hash_conf<>::prime();
    res = res ^ ((addr.s_addr >> i) & 0xFF);
  }
  res = res * hash_conf<>::prime();
  res = res ^ (sa->sin_port >> 1);
  res = res * hash_conf<>::prime();
  res = res ^ (sa->sin_port & 0xFF);
  return res;
}

size_t ep_hash::hash(const sockaddr_in6* sa) const noexcept {
  auto& addr = sa->sin6_addr;
  size_t res = hash_conf<>::basis();
  for (int i = 0; i < 16; ++i) {
    res = res * hash_conf<>::prime();
    res = res ^ addr.s6_addr[i];
  }
  res = res * hash_conf<>::prime();
  res = res ^ (sa->sin6_port >> 1);
  res = res * hash_conf<>::prime();
  res = res ^ (sa->sin6_port & 0xFF);
  return res;
}

bool operator==(const ip_endpoint& lhs, const ip_endpoint& rhs) {
  auto same = false;
  if (*lhs.clength() == *rhs.clength() &&
      lhs.caddress()->sa_family == rhs.caddress()->sa_family) {
    switch (lhs.caddress()->sa_family) {
      case AF_INET: {
        auto* la = reinterpret_cast<const sockaddr_in*>(lhs.caddress());
        auto* ra = reinterpret_cast<const sockaddr_in*>(rhs.caddress());
        same = (0 == memcmp(&la->sin_addr, &ra->sin_addr, sizeof(in_addr)))
               && (la->sin_port == ra->sin_port);
        break;
      }
      case AF_INET6: {
        auto* la = reinterpret_cast<const sockaddr_in6*>(lhs.caddress());
        auto* ra = reinterpret_cast<const sockaddr_in6*>(rhs.caddress());
        same = (0 == memcmp(&la->sin6_addr, &ra->sin6_addr, sizeof(in6_addr)))
               && (la->sin6_port == ra->sin6_port);
        break;
      }
      default:
        break;
    }
  }
  return same;
}

std::string to_string(const ip_endpoint& ep) {
  return host(ep) + ":" + std::to_string(port(ep));
}

std::string host(const ip_endpoint& ep) {
  char addr[INET6_ADDRSTRLEN];
  if (*ep.clength() == 0)
    return "";
  switch(ep.caddress()->sa_family) {
    case AF_INET:
      inet_ntop(AF_INET,
                &const_cast<sockaddr_in*>(reinterpret_cast<const sockaddr_in*>(ep.caddress()))->sin_addr,
                addr, static_cast<socket_size_type>(*ep.clength()));
      break;
    case AF_INET6:
      inet_ntop(AF_INET6,
                &const_cast<sockaddr_in6*>(reinterpret_cast<const sockaddr_in6*>(ep.caddress()))->sin6_addr,
                addr, static_cast<socket_size_type>(*ep.clength()));
      break;
    default:
      addr[0] = '\0';
      break;
  }
  return std::string(addr);
}

uint16_t port(const ip_endpoint& ep) {
  uint16_t port = 0;
  if (*ep.clength() == 0)
    return 0;
  switch(ep.caddress()->sa_family) {
    case AF_INET:
      port = ntohs(reinterpret_cast<const sockaddr_in*>(ep.caddress())->sin_port);
      break;
    case AF_INET6:
      port = ntohs(reinterpret_cast<const sockaddr_in6*>(ep.caddress())->sin6_port);
      break;
    default:
      // nop
      break;
  }
  return port;
}

uint32_t family(const ip_endpoint& ep) {
  if (*ep.clength() == 0)
    return 0;
  return ep.caddress()->sa_family;
}

error load_endpoint(ip_endpoint& ep, uint32_t& f, std::string& h,
                    uint16_t& p, size_t& l) {
  ep.clear();
  if (l > 0) {
    *ep.length() = l;
    switch(f) {
      case AF_INET: {
        auto* addr = reinterpret_cast<sockaddr_in*>(ep.address());
        inet_pton(AF_INET, h.c_str(), &addr->sin_addr);
        addr->sin_port = htons(p);
        addr->sin_family = static_cast<sa_family_t>(f);
        break;
      }
      case AF_INET6: {
        auto* addr = reinterpret_cast<sockaddr_in6*>(ep.address());
        inet_pton(AF_INET6, h.c_str(), &addr->sin6_addr);
        addr->sin6_port = htons(p);
        addr->sin6_family = static_cast<sa_family_t>(f);
        break;
      }
      default:
        return sec::invalid_argument;
    }
  }
  return none;
}

error save_endpoint(ip_endpoint& ep, uint32_t& f, std::string& h,
                    uint16_t& p, size_t& l) {
  if (*ep.length() > 0) {
    f = family(ep);
    h = host(ep);
    p = port(ep);
    l = *ep.length();
  } else {
    f = 0;
    h = "";
    p = 0;
    l = 0;
  }
  return none;
}

} // namespace network
} // namespace io
} // namespace caf
