// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/ipv6_address.hpp"

#include "caf/test/test.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/byte_buffer.hpp"
#include "caf/ipv4_address.hpp"

#include <initializer_list>

using namespace caf;

namespace {

using array_type = ipv6_address::array_type;

ipv6_address addr(std::initializer_list<uint16_t> prefix,
                  std::initializer_list<uint16_t> suffix = {}) {
  return ipv6_address{prefix, suffix};
}

} // namespace

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
  check_eq(from_string("1234:abcd::5678:9abc"),
           addr({0x1234, 0xabcd}, {0x5678, 0x9abc}));
  check_eq(from_string("1234:abcd:1:203:405:607:809:a0b"),
           addr({0x1234, 0xabcd, 1, 0x203, 0x405, 0x607, 0x809, 0xa0b}));
  auto invalid = [](std::string_view str) {
    ipv6_address result;
    auto err = parse(str, result);
    return err != none;
  };
  check(invalid("1:2:3:4:5:6:7:8:9"));
  check(invalid("1:2:3:4::5:6:7:8:9"));
  check(invalid("1:2:3::4:5:6::7:8:9"));
  check(invalid("12345::"));
}

TEST("to string") {
  check_eq(to_string(addr({}, {0x01})), "::1");
  check_eq(to_string(addr({0x01}, {0x01})), "1::1");
  check_eq(to_string(addr({0x01})), "1::");
  check_eq(to_string(addr({}, {0xFFFF, 0x01, 0x01})), "0.1.0.1");
  check_eq(to_string(addr({1, 0, 0, 2, 0, 0, 0, 3})), "1:0:0:2::3");
  check_eq(to_string(addr({1, 0, 0, 2, 0, 0, 3, 4})), "1::2:0:0:3:4");
  check_eq(to_string(addr({1, 2, 3, 0, 4, 5, 6, 7})), "1:2:3:0:4:5:6:7");
}

TEST("IPv4-mapped addresses") {
  auto bytes = array_type{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 255, 1, 2, 3, 4};
  auto v4 = make_ipv4_address(1, 2, 3, 4);
  ipv6_address mapped{v4};
  check_eq(mapped.bytes(), bytes);
  check_eq(ipv6_address{bytes}.embedded_v4(), v4);
  check(mapped.embeds_v4());
  for (size_t i = 0; i < 12; ++i) {
    auto value = mapped;
    value[i] ^= 1;
    check(!value.embeds_v4());
  }
  check(ipv6_address{make_ipv4_address(127, 255, 255, 255)}.is_loopback());
  check(!ipv6_address{make_ipv4_address(128, 0, 0, 0)}.is_loopback());
}

TEST("zero identifies the all-zero address") {
  ipv6_address zero;
  check(zero.zero());
  for (size_t i = 0; i < ipv6_address::num_bytes; ++i) {
    auto value = zero;
    value[i] = 1;
    check(!value.zero());
  }
}

TEST("IPv6 loopback detection requires the full native loopback address") {
  check(ipv6_address::loopback().is_loopback());
  check(!ipv6_address{}.is_loopback());
  check(!addr({}, {2}).is_loopback());
  for (size_t i = 0; i < 15; ++i) {
    auto value = ipv6_address::loopback();
    value[i] = 1;
    check(!value.is_loopback());
  }
}

TEST("IPv6 segment construction stores network-order bytes") {
  auto bytes = array_type{0x12, 0x34, 0xab, 0xcd, 0,    0,    0,    0,
                          0,    0,    0,    0,    0x56, 0x78, 0x9a, 0xbc};
  auto value = addr({0x1234, 0xabcd}, {0x5678, 0x9abc});
  check_eq(value.bytes(), bytes);
}

TEST("binary serialization stores IPv6 as a byte array") {
  auto bytes = array_type{0x12, 0x34, 0xab, 0xcd, 0,    0,    0,    0,
                          0,    0,    0,    0,    0x56, 0x78, 0x9a, 0xbc};
  ipv6_address input{bytes};
  byte_buffer actual;
  byte_buffer expected;
  binary_serializer sink{actual};
  binary_serializer reference{expected};
  require(sink.apply(input));
  require(reference.apply(bytes));
  check_eq(actual, expected);
}

TEST("binary deserialization reads IPv6 from a byte array") {
  auto bytes = array_type{0x12, 0x34, 0xab, 0xcd, 0,    0,    0,    0,
                          0,    0,    0,    0,    0x56, 0x78, 0x9a, 0xbc};
  byte_buffer expected;
  binary_serializer reference{expected};
  require(reference.apply(bytes));
  ipv6_address output;
  binary_deserializer source{expected};
  require(source.apply(output));
  check_eq(output.bytes(), bytes);
}
