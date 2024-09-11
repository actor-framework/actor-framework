// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/ipv6_subnet.hpp"

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"

using namespace caf;

namespace {

ipv6_subnet operator/(ipv6_address addr, uint8_t prefix) {
  return {addr, prefix};
}

} // namespace

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
  check(local.contains(
    ipv6_subnet(ipv6_address({0xbebe, 0xbebe, 0xbebe}, {}), 64)));
  check(local.contains(
    ipv6_subnet(ipv6_address({0xbebe, 0xbebe, 0xbebe}, {}), 32)));
  check(!local.contains(ipv6_subnet(ipv6_address({0xbebe, 0xbebe}, {}), 16)));
  auto local_embed_v4 = ipv6_subnet{make_ipv4_address(127, 0, 0, 1), 8};
  check(local_embed_v4.contains(make_ipv4_address(127, 127, 127, 1)));
  check(!local_embed_v4.contains(make_ipv4_address(128, 0, 0, 1)));
  check(local_embed_v4.contains(
    ipv4_subnet{make_ipv4_address(127, 127, 0, 1), 16}));
  check(!local_embed_v4.contains(
    ipv4_subnet{make_ipv4_address(128, 127, 0, 1), 16}));
}

TEST("embedding") {
  ipv4_subnet v4_local{make_ipv4_address(127, 0, 0, 1), 8};
  ipv6_subnet local{v4_local};
  check(local.embeds_v4());
  check_eq(local.prefix_length(), 104u);
}

TEST("serializing") {
  auto local = ipv6_address{{0xbebe, 0xbebe}, {}} / 32;
  check_eq(to_string(local), "bebe:bebe::/32");
  ipv4_subnet v4_local{make_ipv4_address(127, 0, 0, 1), 8};
  ipv6_subnet local_embed_v4{v4_local};
  check_eq(to_string(local_embed_v4), "127.0.0.0/8");
}

WITH_FIXTURE(test::fixture::deterministic) {

#define CHECK_SERIALIZATION(subn) check_eq(subn, serialization_roundtrip(subn))

TEST("serialization") {
  CHECK_SERIALIZATION((ipv6_address{{0xbebe}, {}} / 128));
  CHECK_SERIALIZATION((ipv6_address{{0xbebe}, {}} / 32));
  CHECK_SERIALIZATION((ipv6_address{{0xbebe}, {0xbebe}} / 32));
  CHECK_SERIALIZATION((ipv6_address{{0xbebe}, {0xbebe}} / 64));
  CHECK_SERIALIZATION((ipv6_address{{}, {0xbebe}} / 16));
  CHECK_SERIALIZATION((ipv6_address{{}, {0xbebe}} / 32));
  CHECK_SERIALIZATION((ipv6_address{{}, {0xbebe, 0xbebe}} / 32));
  CHECK_SERIALIZATION((ipv6_address{{}, {0xbebe, 0xbebe}} / 64));
}

} // WITH_FIXTURE(test::fixture::deterministic)
