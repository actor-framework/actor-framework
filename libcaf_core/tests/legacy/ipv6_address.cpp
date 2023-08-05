// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE ipv6_address

#include "caf/ipv6_address.hpp"

#include "caf/ipv4_address.hpp"

#include "core-test.hpp"

#include <initializer_list>

using namespace caf;

namespace {

using array_type = ipv6_address::array_type;

ipv6_address addr(std::initializer_list<uint16_t> prefix,
                  std::initializer_list<uint16_t> suffix = {}) {
  return ipv6_address{prefix, suffix};
}

} // namespace

CAF_TEST(constructing) {
  ipv6_address::array_type localhost_bytes{
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}};
  ipv6_address localhost{localhost_bytes};
  CHECK_EQ(localhost.data(), localhost_bytes);
  CHECK_EQ(localhost, addr({}, {0x01}));
}

CAF_TEST(comparison) {
  CHECK_EQ(addr({1, 2, 3}), addr({1, 2, 3}));
  CHECK_NE(addr({3, 2, 1}), addr({1, 2, 3}));
  CHECK_EQ(addr({}, {0xFFFF, 0x7F00, 0x0001}), make_ipv4_address(127, 0, 0, 1));
}

CAF_TEST(from string) {
  auto from_string = [](std::string_view str) {
    ipv6_address result;
    auto err = parse(str, result);
    if (err)
      CAF_FAIL("error while parsing " << str << ": " << to_string(err));
    return result;
  };
  CHECK_EQ(from_string("::1"), addr({}, {0x01}));
  CHECK_EQ(from_string("::11"), addr({}, {0x11}));
  CHECK_EQ(from_string("::112"), addr({}, {0x0112}));
  CHECK_EQ(from_string("::1122"), addr({}, {0x1122}));
  CHECK_EQ(from_string("::1:2"), addr({}, {0x01, 0x02}));
  CHECK_EQ(from_string("::1:2"), addr({}, {0x01, 0x02}));
  CHECK_EQ(from_string("1::1"), addr({0x01}, {0x01}));
  CHECK_EQ(from_string("2a00:bdc0:e003::"), addr({0x2a00, 0xbdc0, 0xe003}, {}));
  CHECK_EQ(from_string("1::"), addr({0x01}, {}));
  CHECK_EQ(from_string("0.1.0.1"), addr({}, {0xFFFF, 0x01, 0x01}));
  CHECK_EQ(from_string("::ffff:127.0.0.1"), addr({}, {0xFFFF, 0x7F00, 0x0001}));
  CHECK_EQ(from_string("1:2:3:4:5:6:7:8"), addr({1, 2, 3, 4, 5, 6, 7, 8}));
  CHECK_EQ(from_string("1:2:3:4::5:6:7:8"), addr({1, 2, 3, 4, 5, 6, 7, 8}));
  CHECK_EQ(from_string("1:2:3:4:5:6:0.7.0.8"), addr({1, 2, 3, 4, 5, 6, 7, 8}));
  auto invalid = [](std::string_view str) {
    ipv6_address result;
    auto err = parse(str, result);
    return err != none;
  };
  CHECK(invalid("1:2:3:4:5:6:7:8:9"));
  CHECK(invalid("1:2:3:4::5:6:7:8:9"));
  CHECK(invalid("1:2:3::4:5:6::7:8:9"));
}

CAF_TEST(to string) {
  CHECK_EQ(to_string(addr({}, {0x01})), "::1");
  CHECK_EQ(to_string(addr({0x01}, {0x01})), "1::1");
  CHECK_EQ(to_string(addr({0x01})), "1::");
  CHECK_EQ(to_string(addr({}, {0xFFFF, 0x01, 0x01})), "0.1.0.1");
}
