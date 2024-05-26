// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/ipv6_address.hpp"

#include "caf/test/test.hpp"

#include "caf/ipv4_address.hpp"

#include <initializer_list>

using namespace caf;

namespace {

using array_type = ipv6_address::array_type;

ipv6_address addr(std::initializer_list<uint16_t> prefix,
                  std::initializer_list<uint16_t> suffix = {}) {
  return ipv6_address{prefix, suffix};
}

TEST("constructing") {
  ipv6_address::array_type localhost_bytes{
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}};
  ipv6_address localhost{localhost_bytes};
  check_eq(localhost.data(), localhost_bytes);
  check_eq(localhost, addr({}, {0x01}));
}

TEST("comparison") {
  check_eq(addr({1, 2, 3}), addr({1, 2, 3}));
  check_ne(addr({3, 2, 1}), addr({1, 2, 3}));
  check_eq(addr({}, {0xFFFF, 0x7F00, 0x0001}), make_ipv4_address(127, 0, 0, 1));
}

TEST("from string") {
  auto from_string = [this](std::string_view str) {
    ipv6_address result;
    auto err = parse(str, result);
    if (err)
      fail("error while parsing {}: {}", str, to_string(err));
    return result;
  };
  check_eq(from_string("::1"), addr({}, {0x01}));
  check_eq(from_string("::11"), addr({}, {0x11}));
  check_eq(from_string("::112"), addr({}, {0x0112}));
  check_eq(from_string("::1122"), addr({}, {0x1122}));
  check_eq(from_string("::1:2"), addr({}, {0x01, 0x02}));
  check_eq(from_string("::1:2"), addr({}, {0x01, 0x02}));
  check_eq(from_string("1::1"), addr({0x01}, {0x01}));
  check_eq(from_string("2a00:bdc0:e003::"), addr({0x2a00, 0xbdc0, 0xe003}, {}));
  check_eq(from_string("1::"), addr({0x01}, {}));
  check_eq(from_string("0.1.0.1"), addr({}, {0xFFFF, 0x01, 0x01}));
  check_eq(from_string("::ffff:127.0.0.1"), addr({}, {0xFFFF, 0x7F00, 0x0001}));
  check_eq(from_string("1:2:3:4:5:6:7:8"), addr({1, 2, 3, 4, 5, 6, 7, 8}));
  check_eq(from_string("1:2:3:4::5:6:7:8"), addr({1, 2, 3, 4, 5, 6, 7, 8}));
  check_eq(from_string("1:2:3:4:5:6:0.7.0.8"), addr({1, 2, 3, 4, 5, 6, 7, 8}));
  auto invalid = [](std::string_view str) {
    ipv6_address result;
    auto err = parse(str, result);
    return err != none;
  };
  check(invalid("1:2:3:4:5:6:7:8:9"));
  check(invalid("1:2:3:4::5:6:7:8:9"));
  check(invalid("1:2:3::4:5:6::7:8:9"));
}

TEST("to string") {
  check_eq(to_string(addr({}, {0x01})), "::1");
  check_eq(to_string(addr({0x01}, {0x01})), "1::1");
  check_eq(to_string(addr({0x01})), "1::");
  check_eq(to_string(addr({}, {0xFFFF, 0x01, 0x01})), "0.1.0.1");
}

} // namespace
