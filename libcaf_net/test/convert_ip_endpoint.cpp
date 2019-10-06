/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#define CAF_SUITE convert_ip_endpoint

#include "caf/detail/convert_ip_endpoint.hpp"

#include "caf/net/test/host_fixture.hpp"
#include "caf/test/dsl.hpp"

#include <cstring>

#include "caf/detail/socket_sys_includes.hpp"
#include "caf/ipv4_endpoint.hpp"
#include "caf/ipv6_endpoint.hpp"

using namespace caf;
using namespace caf::detail;

namespace {

struct fixture : host_fixture {
  fixture() : host_fixture() {
    memset(&sockaddr6_src, 0, sizeof(sockaddr_in6));
    memset(&sockaddr6_dst, 0, sizeof(sockaddr_in6));
    sockaddr6_src.sin6_family = AF_INET6;
    sockaddr6_src.sin6_port = htons(23);
    sockaddr6_src.sin6_addr = in6addr_loopback;
    memset(&sockaddr4_src, 0, sizeof(sockaddr_in));
    memset(&sockaddr4_dst, 0, sizeof(sockaddr_in));
    sockaddr4_src.sin_family = AF_INET;
    sockaddr4_src.sin_port = htons(23);
    sockaddr4_src.sin_addr.s_addr = INADDR_LOOPBACK;
  }

  sockaddr_in6 sockaddr6_src;
  sockaddr_in6 sockaddr6_dst;
  sockaddr_in sockaddr4_src;
  sockaddr_in sockaddr4_dst;
  ip_endpoint ep_src;
  ip_endpoint ep_dst;
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(convert_ip_endpoint_tests, fixture)

CAF_TEST(sockaddr_in6 roundtrip) {
  ip_endpoint ep;
  CAF_MESSAGE("converting sockaddr_in6 to ip_endpoint");
  CAF_CHECK_EQUAL(convert(reinterpret_cast<sockaddr_storage&>(sockaddr6_src),
                          ep),
                  none);
  CAF_MESSAGE("converting ip_endpoint to sockaddr_in6");
  convert(ep, reinterpret_cast<sockaddr_storage&>(sockaddr6_dst));
  CAF_CHECK_EQUAL(memcmp(&sockaddr6_src, &sockaddr6_dst, sizeof(sockaddr_in6)),
                  0);
}

CAF_TEST(ipv6_endpoint roundtrip) {
  sockaddr_storage addr;
  memset(&addr, 0, sizeof(sockaddr_storage));
  if (auto err = detail::parse("[::1]:55555", ep_src))
    CAF_FAIL("unable to parse input: " << err);
  CAF_MESSAGE("converting ip_endpoint to sockaddr_in6");
  convert(ep_src, addr);
  CAF_MESSAGE("converting sockaddr_in6 to ip_endpoint");
  CAF_CHECK_EQUAL(convert(addr, ep_dst), none);
  CAF_CHECK_EQUAL(ep_src, ep_dst);
}

CAF_TEST(sockaddr_in4 roundtrip) {
  ip_endpoint ep;
  CAF_MESSAGE("converting sockaddr_in to ip_endpoint");
  CAF_CHECK_EQUAL(convert(reinterpret_cast<sockaddr_storage&>(sockaddr4_src),
                          ep),
                  none);
  CAF_MESSAGE("converting ip_endpoint to sockaddr_in");
  convert(ep, reinterpret_cast<sockaddr_storage&>(sockaddr4_dst));
  CAF_CHECK_EQUAL(memcmp(&sockaddr4_src, &sockaddr4_dst, sizeof(sockaddr_in)),
                  0);
}

CAF_TEST(ipv4_endpoint roundtrip) {
  sockaddr_storage addr;
  memset(&addr, 0, sizeof(sockaddr_storage));
  if (auto err = detail::parse("127.0.0.1:55555", ep_src))
    CAF_FAIL("unable to parse input: " << err);
  CAF_MESSAGE("converting ip_endpoint to sockaddr_in");
  convert(ep_src, addr);
  CAF_MESSAGE("converting sockaddr_in to ip_endpoint");
  CAF_CHECK_EQUAL(convert(addr, ep_dst), none);
  CAF_CHECK_EQUAL(ep_src, ep_dst);
}

CAF_TEST_FIXTURE_SCOPE_END();
