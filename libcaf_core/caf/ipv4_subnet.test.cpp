// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/ipv4_subnet.hpp"

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"

using namespace caf;

namespace {

const auto addr = make_ipv4_address;

ipv4_subnet operator/(ipv4_address addr, uint8_t prefix) {
  return {addr, prefix};
}

TEST("constructing") {
  ipv4_subnet zero{addr(0, 0, 0, 0), 32};
  check_eq(zero.network_address(), addr(0, 0, 0, 0));
  check_eq(zero.prefix_length(), 32u);
  ipv4_subnet local{addr(127, 0, 0, 0), 8};
  check_eq(local.network_address(), addr(127, 0, 0, 0));
  check_eq(local.prefix_length(), 8u);
}

TEST("equality") {
  auto a = addr(0xff, 0xff, 0xff, 0xff) / 19;
  auto b = addr(0xff, 0xff, 0xff, 0xab) / 19;
  auto net = addr(0xff, 0xff, 0xe0, 0x00);
  check_eq(a.network_address(), net);
  check_eq(a.network_address(), b.network_address());
  check_eq(a.prefix_length(), b.prefix_length());
  check_eq(a, b);
}

TEST("contains") {
  ipv4_subnet local{addr(127, 0, 0, 0), 8};
  check(local.contains(addr(127, 0, 0, 1)));
  check(local.contains(addr(127, 1, 2, 3)));
  check(local.contains(addr(127, 128, 0, 0) / 9));
  check(local.contains(addr(127, 0, 0, 0) / 8));
  check(!local.contains(addr(127, 0, 0, 0) / 7));
}

TEST("ordering") {
  check_eq(addr(192, 168, 168, 0) / 24, addr(192, 168, 168, 0) / 24);
  check_ne(addr(192, 168, 168, 0) / 25, addr(192, 168, 168, 0) / 24);
  check_lt(addr(192, 168, 167, 0) / 24, addr(192, 168, 168, 0) / 24);
  check_lt(addr(192, 168, 168, 0) / 24, addr(192, 168, 168, 0) / 25);
}

WITH_FIXTURE(test::fixture::deterministic) {

#define CHECK_SERIALIZATION(subn) check_eq(subn, serialization_roundtrip(subn))

TEST("serialization") {
  CHECK_SERIALIZATION(addr(192, 168, 168, 1) / 16);
  CHECK_SERIALIZATION(addr(192, 168, 168, 1) / 24);
  CHECK_SERIALIZATION(addr(192, 168, 168, 1) / 31);
  CHECK_SERIALIZATION(addr(255, 255, 255, 1) / 8);
  CHECK_SERIALIZATION(addr(127, 0, 0, 1) / 24);
}

} // WITH_FIXTURE(test::fixture::deterministic)

} // namespace
