// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/io/network/ip_endpoint.hpp"

#include "caf/io/network/native_socket.hpp"

#include "caf/hash/fnv.hpp"
#include "caf/log/system.hpp"
#include "caf/sec.hpp"

// clang-format off
#ifdef CAF_WINDOWS
#  include <winsock2.h>
#  include <windows.h>
#  include <ws2tcpip.h>
#  include <ws2ipdef.h>
#else
#  include <sys/types.h>
#  include <arpa/inet.h>
#  include <cerrno>
#  include <netinet/in.h>
#  include <netinet/ip.h>
#  include <sys/socket.h>
#  include <unistd.h>
#endif
// clang-format on

#ifdef CAF_WINDOWS
using sa_family_t = short;
#endif

namespace caf::io::network {

struct ip_endpoint::impl {
  sockaddr_storage addr;
  size_t len = 0;
  impl() {
    memset(&addr, 0, sizeof(sockaddr_storage));
  }
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
  if (this != &other) {
    ptr_.reset(new ip_endpoint::impl);
    memcpy(address(), other.caddress(), sizeof(sockaddr_storage));
    *length() = *other.clength();
  }
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

void ip_endpoint::impl_deleter::operator()(ip_endpoint::impl* ptr) const {
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
      log::system::error("failed to hash socket address: "
                         "only IPv4 and IPv6 are supported");
      return 0;
  }
}

size_t ep_hash::hash(const sockaddr_in* sa) const noexcept {
  return hash::fnv<size_t>::compute(sa->sin_addr.s_addr, sa->sin_port);
}

size_t ep_hash::hash(const sockaddr_in6* sa) const noexcept {
  auto bytes = as_bytes(make_span(sa->sin6_addr.s6_addr));
  return hash::fnv<size_t>::compute(bytes, sa->sin6_port);
}

bool operator==(const ip_endpoint& lhs, const ip_endpoint& rhs) {
  if (*lhs.clength() == 0 && *rhs.clength() == 0)
    return true;
  if (*lhs.clength() != *rhs.clength())
    return false;
  if (lhs.caddress()->sa_family != rhs.caddress()->sa_family)
    return false;
  switch (lhs.caddress()->sa_family) {
    case AF_INET: {
      auto* la = reinterpret_cast<const sockaddr_in*>(lhs.caddress());
      auto* ra = reinterpret_cast<const sockaddr_in*>(rhs.caddress());
      return memcmp(&la->sin_addr, &ra->sin_addr, sizeof(in_addr)) == 0
             && (la->sin_port == ra->sin_port);
    }
    case AF_INET6: {
      auto* la = reinterpret_cast<const sockaddr_in6*>(lhs.caddress());
      auto* ra = reinterpret_cast<const sockaddr_in6*>(rhs.caddress());
      return memcmp(&la->sin6_addr, &ra->sin6_addr, sizeof(in6_addr)) == 0
             && (la->sin6_port == ra->sin6_port);
    }
    default:
      return false;
  }
}

std::string to_string(const ip_endpoint& ep) {
  std::string result;
  if (is_ipv6(ep)) {
    result += '[';
    result += host(ep);
    result += ']';
  } else {
    result += host(ep);
  }
  result += ':';
  result += std::to_string(port(ep));
  return result;
}

std::string host(const ip_endpoint& ep) {
  char addr[INET6_ADDRSTRLEN];
  if (*ep.clength() == 0)
    return "";
  switch (ep.caddress()->sa_family) {
    case AF_INET:
      inet_ntop(AF_INET,
                &const_cast<sockaddr_in*>(
                   reinterpret_cast<const sockaddr_in*>(ep.caddress()))
                   ->sin_addr,
                addr, static_cast<socket_size_type>(*ep.clength()));
      break;
    case AF_INET6:
      inet_ntop(AF_INET6,
                &const_cast<sockaddr_in6*>(
                   reinterpret_cast<const sockaddr_in6*>(ep.caddress()))
                   ->sin6_addr,
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
  switch (ep.caddress()->sa_family) {
    case AF_INET:
      port
        = ntohs(reinterpret_cast<const sockaddr_in*>(ep.caddress())->sin_port);
      break;
    case AF_INET6:
      port = ntohs(
        reinterpret_cast<const sockaddr_in6*>(ep.caddress())->sin6_port);
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

bool is_ipv4(const ip_endpoint& ep) {
  return family(ep) == AF_INET;
}

bool is_ipv6(const ip_endpoint& ep) {
  return family(ep) == AF_INET6;
}

error_code<sec> load_endpoint(ip_endpoint& ep, uint32_t& f, std::string& h,
                              uint16_t& p, size_t& l) {
  ep.clear();
  if (l > 0) {
    *ep.length() = l;
    switch (f) {
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

error_code<sec> save_endpoint(ip_endpoint& ep, uint32_t& f, std::string& h,
                              uint16_t& p, size_t& l) {
  if (*ep.length() > 0) {
    f = family(ep);
    h = host(ep);
    p = port(ep);
    l = *ep.length();
  } else {
    f = 0;
    h.clear();
    p = 0;
    l = 0;
  }
  return none;
}

} // namespace caf::io::network
