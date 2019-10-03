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

#include "caf/test/dsl.hpp"

#include "host_fixture.hpp"

#include <cstring>

#include "caf/detail/socket_sys_includes.hpp"
#include "caf/ipv4_endpoint.hpp"
#include "caf/ipv6_endpoint.hpp"

using namespace caf;
using namespace caf::detail;

namespace {

struct fixture : host_fixture {
  fixture() : host_fixture() {
    memset(&sockaddr4_src, 0, sizeof(sockaddr_storage));
    memset(&sockaddr6_src, 0, sizeof(sockaddr_storage));
    auto sockaddr6_ptr = reinterpret_cast<sockaddr_in6*>(&sockaddr6_src);
    sockaddr6_ptr->sin6_family = AF_INET6;
    sockaddr6_ptr->sin6_port = htons(23);
    sockaddr6_ptr->sin6_addr = in6addr_loopback;
    auto sockaddr4_ptr = reinterpret_cast<sockaddr_in*>(&sockaddr4_src);
    sockaddr4_ptr->sin_family = AF_INET;
    sockaddr4_ptr->sin_port = htons(23);
    sockaddr4_ptr->sin_addr.s_addr = INADDR_LOOPBACK;
  }

  sockaddr_storage sockaddr6_src;
  sockaddr_storage sockaddr4_src;
  sockaddr_storage dst;
  ip_endpoint ep_src;
  ip_endpoint ep_dst;
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(convert_ip_endpoint_tests, fixture)

CAF_TEST(sockaddr_in6 roundtrip) {
  ip_endpoint tmp;
  CAF_MESSAGE("converting sockaddr_in6 to ip_endpoint");
  CAF_CHECK_EQUAL(convert(sockaddr6_src, tmp), none);
  CAF_MESSAGE("converting ip_endpoint to sockaddr_in6");
  convert(tmp, dst);
  CAF_CHECK_EQUAL(memcmp(&sockaddr6_src, &dst, sizeof(sockaddr_storage)), 0);
}

CAF_TEST(ipv6_endpoint roundtrip) {
  sockaddr_storage tmp = {};
  if (auto err = detail::parse("[::1]:55555", ep_src))
    CAF_FAIL("unable to parse input: " << err);
  CAF_MESSAGE("converting ip_endpoint to sockaddr_in6");
  convert(ep_src, tmp);
  CAF_MESSAGE("converting sockaddr_in6 to ip_endpoint");
  CAF_CHECK_EQUAL(convert(tmp, ep_dst), none);
  CAF_CHECK_EQUAL(ep_src, ep_dst);
}

CAF_TEST(sockaddr_in4 roundtrip) {
  ip_endpoint tmp;
  CAF_MESSAGE("converting sockaddr_in to ip_endpoint");
  CAF_CHECK_EQUAL(convert(sockaddr4_src, tmp), none);
  CAF_MESSAGE("converting ip_endpoint to sockaddr_in");
  convert(tmp, dst);
  CAF_CHECK_EQUAL(memcmp(&sockaddr4_src, &dst, sizeof(sockaddr_storage)), 0);
}

CAF_TEST(ipv4_endpoint roundtrip) {
  sockaddr_storage tmp = {};
  if (auto err = detail::parse("127.0.0.1:55555", ep_src))
    CAF_FAIL("unable to parse input: " << err);
  CAF_MESSAGE("converting ip_endpoint to sockaddr_in");
  convert(ep_src, tmp);
  CAF_MESSAGE("converting sockaddr_in to ip_endpoint");
  CAF_CHECK_EQUAL(convert(tmp, ep_dst), none);
  CAF_CHECK_EQUAL(ep_src, ep_dst);
}

CAF_TEST_FIXTURE_SCOPE_END()
