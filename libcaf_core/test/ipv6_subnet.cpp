// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE ipv6_subnet

#include "caf/ipv6_subnet.hpp"

#include "core-test.hpp"

using namespace caf;

namespace {

ipv6_subnet operator/(ipv6_address addr, uint8_t prefix) {
  return {addr, prefix};
}

} // namespace

CAF_TEST(constructing) {
  auto zero = ipv6_address() / 128;
  CHECK_EQ(zero.network_address(), ipv6_address());
  CHECK_EQ(zero.prefix_length(), 128u);
}

CAF_TEST(equality) {
  auto a = ipv6_address{{0xffff, 0xffff, 0xffff}, {}} / 27;
  auto b = ipv6_address{{0xffff, 0xffff, 0xabab}, {}} / 27;
  auto net = ipv6_address{{0xffff, 0xffe0}, {}};
  CHECK_EQ(a.network_address(), net);
  CHECK_EQ(a.network_address(), b.network_address());
  CHECK_EQ(a.prefix_length(), b.prefix_length());
  CHECK_EQ(a, b);
}

CAF_TEST(contains) {
  auto local = ipv6_address{{0xbebe, 0xbebe}, {}} / 32;
  CHECK(local.contains(ipv6_address({0xbebe, 0xbebe, 0xbebe}, {})));
  CHECK(!local.contains(ipv6_address({0xbebe, 0xbebf}, {})));
}

CAF_TEST(embedding) {
  ipv4_subnet v4_local{make_ipv4_address(127, 0, 0, 1), 8};
  ipv6_subnet local{v4_local};
  CHECK(local.embeds_v4());
  CHECK_EQ(local.prefix_length(), 104u);
}
