// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/ipv6_subnet.hpp"

#include "caf/test/test.hpp"

using namespace caf;

namespace {

ipv6_subnet operator/(ipv6_address addr, uint8_t prefix) {
  return {addr, prefix};
}

TEST("constructing") {
  auto zero = ipv6_address() / 128;
  check_eq(zero.network_address(), ipv6_address());
  check_eq(zero.prefix_length(), 128u);
}

TEST("equality") {
  auto a = ipv6_address{{0xffff, 0xffff, 0xffff}, {}} / 27;
  auto b = ipv6_address{{0xffff, 0xffff, 0xabab}, {}} / 27;
  auto net = ipv6_address{{0xffff, 0xffe0}, {}};
  check_eq(a.network_address(), net);
  check_eq(a.network_address(), b.network_address());
  check_eq(a.prefix_length(), b.prefix_length());
  check_eq(a, b);
}

TEST("contains") {
  auto local = ipv6_address{{0xbebe, 0xbebe}, {}} / 32;
  check(local.contains(ipv6_address({0xbebe, 0xbebe, 0xbebe}, {})));
  check(!local.contains(ipv6_address({0xbebe, 0xbebf}, {})));
}

TEST("embedding") {
  ipv4_subnet v4_local{make_ipv4_address(127, 0, 0, 1), 8};
  ipv6_subnet local{v4_local};
  check(local.embeds_v4());
  check_eq(local.prefix_length(), 104u);
}

} // namespace
