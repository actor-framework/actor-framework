// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE ipv4_subnet

#include "caf/ipv4_subnet.hpp"

#include "core-test.hpp"

using namespace caf;

namespace {

const auto addr = make_ipv4_address;

ipv4_subnet operator/(ipv4_address addr, uint8_t prefix) {
  return {addr, prefix};
}

} // namespace

CAF_TEST(constructing) {
  ipv4_subnet zero{addr(0, 0, 0, 0), 32};
  CAF_CHECK_EQUAL(zero.network_address(), addr(0, 0, 0, 0));
  CAF_CHECK_EQUAL(zero.prefix_length(), 32u);
  ipv4_subnet local{addr(127, 0, 0, 0), 8};
  CAF_CHECK_EQUAL(local.network_address(), addr(127, 0, 0, 0));
  CAF_CHECK_EQUAL(local.prefix_length(), 8u);
}

CAF_TEST(equality) {
  auto a = addr(0xff, 0xff, 0xff, 0xff) / 19;
  auto b = addr(0xff, 0xff, 0xff, 0xab) / 19;
  auto net = addr(0xff, 0xff, 0xe0, 0x00);
  CAF_CHECK_EQUAL(a.network_address(), net);
  CAF_CHECK_EQUAL(a.network_address(), b.network_address());
  CAF_CHECK_EQUAL(a.prefix_length(), b.prefix_length());
  CAF_CHECK_EQUAL(a, b);
}

CAF_TEST(contains) {
  ipv4_subnet local{addr(127, 0, 0, 0), 8};
  CAF_CHECK(local.contains(addr(127, 0, 0, 1)));
  CAF_CHECK(local.contains(addr(127, 1, 2, 3)));
  CAF_CHECK(local.contains(addr(127, 128, 0, 0) / 9));
  CAF_CHECK(local.contains(addr(127, 0, 0, 0) / 8));
  CAF_CHECK(!local.contains(addr(127, 0, 0, 0) / 7));
}

CAF_TEST(ordering) {
  CAF_CHECK_EQUAL(addr(192, 168, 168, 0) / 24, addr(192, 168, 168, 0) / 24);
  CAF_CHECK_NOT_EQUAL(addr(192, 168, 168, 0) / 25, addr(192, 168, 168, 0) / 24);
  CAF_CHECK_LESS(addr(192, 168, 167, 0) / 24, addr(192, 168, 168, 0) / 24);
  CAF_CHECK_LESS(addr(192, 168, 168, 0) / 24, addr(192, 168, 168, 0) / 25);
}
