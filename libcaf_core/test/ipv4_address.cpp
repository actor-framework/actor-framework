// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE ipv4_address

#include "caf/ipv4_address.hpp"

#include "core-test.hpp"

#include "caf/detail/network_order.hpp"

using caf::detail::to_network_order;

using namespace caf;

namespace {

const auto addr = make_ipv4_address;

} // namespace

CAF_TEST(constructing) {
  auto localhost = addr(127, 0, 0, 1);
  CHECK_EQ(localhost.bits(), to_network_order(0x7F000001u));
  ipv4_address zero;
  CHECK_EQ(zero.bits(), 0u);
}

CAF_TEST(to string) {
  CHECK_EQ(to_string(addr(255, 255, 255, 255)), "255.255.255.255");
}

CAF_TEST(from string - valid inputs) {
  auto from_string = [](std::string_view str) -> expected<ipv4_address> {
    ipv4_address result;
    if (auto err = parse(str, result))
      return err;
    return result;
  };
  CHECK_EQ(from_string("136.12.12.12"), addr(136, 12, 12, 12));
  CHECK_EQ(from_string("255.255.255.255"), addr(255, 255, 255, 255));
}

CAF_TEST(from string - invalid inputs) {
  auto should_fail = [](std::string_view str) {
    ipv4_address result;
    auto err = parse(str, result);
    if (!err)
      CAF_ERROR("error while parsing "
                << str << ", expected an error but got: " << to_string(result));
  };
  should_fail("256.12.12.12");
  should_fail("1136.12.12.12");
  should_fail("1137.12.12.12");
  should_fail("1279.12.12.12");
  should_fail("1280.12.12.12");
}

CAF_TEST(properties) {
  CHECK_EQ(addr(127, 0, 0, 1).is_loopback(), true);
  CHECK_EQ(addr(127, 0, 0, 254).is_loopback(), true);
  CHECK_EQ(addr(127, 0, 1, 1).is_loopback(), true);
  CHECK_EQ(addr(128, 0, 0, 1).is_loopback(), false);
  // Checks multicast according to BCP 51, Section 3.
  CHECK_EQ(addr(223, 255, 255, 255).is_multicast(), false);
  // 224.0.0.0 - 224.0.0.255       (/24)      Local Network Control Block
  CHECK_EQ(addr(224, 0, 0, 1).is_multicast(), true);
  CHECK_EQ(addr(224, 0, 0, 255).is_multicast(), true);
  // 224.0.1.0 - 224.0.1.255       (/24)      Internetwork Control Block
  CHECK_EQ(addr(224, 0, 1, 0).is_multicast(), true);
  CHECK_EQ(addr(224, 0, 1, 255).is_multicast(), true);
  // 224.0.2.0 - 224.0.255.255     (65024)    AD-HOC Block I
  CHECK_EQ(addr(224, 0, 2, 0).is_multicast(), true);
  CHECK_EQ(addr(224, 0, 255, 255).is_multicast(), true);
  // 224.1.0.0 - 224.1.255.255     (/16)      RESERVED
  CHECK_EQ(addr(224, 1, 0, 0).is_multicast(), true);
  CHECK_EQ(addr(224, 1, 255, 255).is_multicast(), true);
  // 224.2.0.0 - 224.2.255.255     (/16)      SDP/SAP Block
  CHECK_EQ(addr(224, 2, 0, 0).is_multicast(), true);
  CHECK_EQ(addr(224, 2, 255, 255).is_multicast(), true);
  // 224.3.0.0 - 224.4.255.255     (2 /16s)   AD-HOC Block II
  CHECK_EQ(addr(224, 3, 0, 0).is_multicast(), true);
  CHECK_EQ(addr(224, 4, 255, 255).is_multicast(), true);
  // 224.5.0.0 - 224.255.255.255   (251 /16s) RESERVED
  CHECK_EQ(addr(224, 5, 0, 0).is_multicast(), true);
  CHECK_EQ(addr(224, 255, 255, 255).is_multicast(), true);
  // 225.0.0.0 - 231.255.255.255   (7 /8s)    RESERVED
  CHECK_EQ(addr(225, 0, 0, 0).is_multicast(), true);
  CHECK_EQ(addr(231, 255, 255, 255).is_multicast(), true);
  // 232.0.0.0 - 232.255.255.255   (/8)       Source-Specific Multicast Block
  CHECK_EQ(addr(232, 0, 0, 0).is_multicast(), true);
  CHECK_EQ(addr(232, 255, 255, 255).is_multicast(), true);
  // 233.0.0.0 - 233.251.255.255   (16515072) GLOP Block
  CHECK_EQ(addr(233, 0, 0, 0).is_multicast(), true);
  CHECK_EQ(addr(233, 251, 255, 255).is_multicast(), true);
  // 233.252.0.0 - 233.255.255.255 (/14)      AD-HOC Block III
  CHECK_EQ(addr(233, 252, 0, 0).is_multicast(), true);
  CHECK_EQ(addr(233, 255, 255, 255).is_multicast(), true);
  // 234.0.0.0 - 238.255.255.255   (5 /8s)    RESERVED
  CHECK_EQ(addr(234, 0, 0, 0).is_multicast(), true);
  CHECK_EQ(addr(238, 255, 255, 255).is_multicast(), true);
  // 239.0.0.0 - 239.255.255.255   (/8)       Administratively Scoped Block
  CHECK_EQ(addr(239, 0, 0, 0).is_multicast(), true);
  CHECK_EQ(addr(239, 255, 255, 255).is_multicast(), true);
  // One above.
  CHECK_EQ(addr(240, 0, 0, 0).is_multicast(), false);
}

CAF_TEST(network addresses) {
  auto all1 = addr(255, 255, 255, 255);
  CHECK_EQ(all1.network_address(0), addr(0x00, 0x00, 0x00, 0x00));
  CHECK_EQ(all1.network_address(7), addr(0xFE, 0x00, 0x00, 0x00));
  CHECK_EQ(all1.network_address(8), addr(0xFF, 0x00, 0x00, 0x00));
  CHECK_EQ(all1.network_address(9), addr(0xFF, 0x80, 0x00, 0x00));
  CHECK_EQ(all1.network_address(31), addr(0xFF, 0xFF, 0xFF, 0xFE));
  CHECK_EQ(all1.network_address(32), addr(0xFF, 0xFF, 0xFF, 0xFF));
  CHECK_EQ(all1.network_address(33), addr(0xFF, 0xFF, 0xFF, 0xFF));
}

CAF_TEST(operators) {
  CHECK_EQ(addr(16, 0, 0, 8) & addr(255, 2, 4, 6), addr(16, 0, 0, 0));
  CHECK_EQ(addr(16, 0, 0, 8) | addr(255, 2, 4, 6), addr(255, 2, 4, 14));
  CHECK_EQ(addr(16, 0, 0, 8) ^ addr(255, 2, 4, 6), addr(239, 2, 4, 14));
}
