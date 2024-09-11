// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/ip.hpp"

#include "caf/test/test.hpp"

#include "caf/ip_address.hpp"
#include "caf/ipv4_address.hpp"

using namespace caf;
using namespace caf::net;

namespace {

struct fixture {
  fixture() : v6_local{{0}, {0x1}} {
    v4_local = ip_address{make_ipv4_address(127, 0, 0, 1)};
    v4_any_addr = ip_address{make_ipv4_address(0, 0, 0, 0)};
  }

  bool contains(ip_address x) {
    return std::count(addrs.begin(), addrs.end(), x) > 0;
  }

  ip_address v4_any_addr;
  ip_address v6_any_addr;
  ip_address v4_local;
  ip_address v6_local;

  std::vector<ip_address> addrs;
};

} // namespace

WITH_FIXTURE(fixture) {

TEST("resolve localhost") {
  addrs = ip::resolve("localhost");
  check(!addrs.empty());
  check(contains(v4_local) || contains(v6_local));
} // WITH_FIXTURE(fixture)

TEST("resolve any") {
  addrs = ip::resolve("");
  check(!addrs.empty());
  check(contains(v4_any_addr) || contains(v6_any_addr));
}

TEST("local addresses localhost") {
  addrs = ip::local_addresses("localhost");
  check(!addrs.empty());
  check(contains(v4_local) || contains(v6_local));
}

TEST("local addresses any") {
  addrs = ip::local_addresses("0.0.0.0");
  auto tmp = ip::local_addresses("::");
  addrs.insert(addrs.end(), tmp.begin(), tmp.end());
  check(!addrs.empty());
  check(contains(v4_any_addr) || contains(v6_any_addr));
}

} // WITH_FIXTURE(fixture)
