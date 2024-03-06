// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/convert_ip_endpoint.hpp"

#include "caf/test/test.hpp"

#include "caf/detail/socket_sys_includes.hpp"
#include "caf/ipv4_endpoint.hpp"
#include "caf/ipv6_endpoint.hpp"
#include "caf/log/test.hpp"

#include <cstring>

using namespace caf;
using namespace caf::detail;

namespace {

struct fixture {
  fixture() {
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

WITH_FIXTURE(fixture) {

TEST("sockaddr_in6 roundtrip") {
  ip_endpoint tmp;
  log::test::debug("converting sockaddr_in6 to ip_endpoint");
  check_eq(convert(sockaddr6_src, tmp), none);
  log::test::debug("converting ip_endpoint to sockaddr_in6");
  convert(tmp, dst);
  check_eq(memcmp(&sockaddr6_src, &dst, sizeof(sockaddr_storage)), 0);
}

TEST("ipv6_endpoint roundtrip") {
  sockaddr_storage tmp = {};
  if (auto err = detail::parse("[::1]:55555", ep_src))
    fail("unable to parse input: {}", err);
  log::test::debug("converting ip_endpoint to sockaddr_in6");
  convert(ep_src, tmp);
  log::test::debug("converting sockaddr_in6 to ip_endpoint");
  check_eq(convert(tmp, ep_dst), none);
  check_eq(ep_src, ep_dst);
}

TEST("sockaddr_in4 roundtrip") {
  ip_endpoint tmp;
  log::test::debug("converting sockaddr_in to ip_endpoint");
  check_eq(convert(sockaddr4_src, tmp), none);
  log::test::debug("converting ip_endpoint to sockaddr_in");
  convert(tmp, dst);
  check_eq(memcmp(&sockaddr4_src, &dst, sizeof(sockaddr_storage)), 0);
}

TEST("ipv4_endpoint roundtrip") {
  sockaddr_storage tmp = {};
  if (auto err = detail::parse("127.0.0.1:55555", ep_src))
    fail("unable to parse input: {}", err);
  log::test::debug("converting ip_endpoint to sockaddr_in");
  convert(ep_src, tmp);
  log::test::debug("converting sockaddr_in to ip_endpoint");
  check_eq(convert(tmp, ep_dst), none);
  check_eq(ep_src, ep_dst);
}

} // WITH_FIXTURE(fixture)

} // namespace
