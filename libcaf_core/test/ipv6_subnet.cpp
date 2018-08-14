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

#include "caf/config.hpp"

#define CAF_SUITE ipv6_subnet
#include "caf/test/dsl.hpp"

#include "caf/ipv6_subnet.hpp"

using namespace caf;

namespace {

ipv6_subnet operator/(ipv6_address addr, uint8_t prefix) {
  return {addr, prefix};
}

} // namespace <anonymous>

CAF_TEST(constructing) {
  auto zero = ipv6_address() / 128;
  CAF_CHECK_EQUAL(zero.network_address(), ipv6_address());
  CAF_CHECK_EQUAL(zero.prefix_length(), 128u);
}

CAF_TEST(equality) {
  auto a = ipv6_address{{0xffff, 0xffff, 0xffff}, {}} / 27;
  auto b = ipv6_address{{0xffff, 0xffff, 0xabab}, {}} / 27;
  auto net = ipv6_address{{0xffff, 0xffe0}, {}};
  CAF_CHECK_EQUAL(a.network_address(), net);
  CAF_CHECK_EQUAL(a.network_address(), b.network_address());
  CAF_CHECK_EQUAL(a.prefix_length(), b.prefix_length());
  CAF_CHECK_EQUAL(a, b);
}

CAF_TEST(constains) {
  auto local = ipv6_address{{0xbebe, 0xbebe}, {}} / 32;
  CAF_CHECK(local.contains(ipv6_address({0xbebe, 0xbebe, 0xbebe}, {})));
  CAF_CHECK(!local.contains(ipv6_address({0xbebe, 0xbebf}, {})));
}

CAF_TEST(embedding) {
  ipv4_subnet v4_local{make_ipv4_address(127, 0, 0, 1), 8};
  ipv6_subnet local{v4_local};
  CAF_CHECK(local.embeds_v4());
  CAF_CHECK_EQUAL(local.prefix_length(), 104u);
}

