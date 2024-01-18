// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/ipv6_endpoint.hpp"

#include "caf/test/caf_test_main.hpp"
#include "caf/test/test.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/ipv4_address.hpp"
#include "caf/ipv4_endpoint.hpp"
#include "caf/ipv6_address.hpp"
#include "caf/span.hpp"

#include <cassert>
#include <vector>

using namespace caf;

namespace {

ipv6_endpoint operator"" _ep(const char* str, size_t size) {
  ipv6_endpoint result;
  if (auto err = detail::parse(std::string_view{str, size}, result))
    test::runnable::current().fail("unable to parse input: {}", err);
  return result;
}

struct fixture {
  actor_system_config cfg;
  actor_system sys{cfg};

  template <class T>
  T roundtrip(T x) {
    byte_buffer buf;
    binary_serializer sink(sys, buf);
    if (!sink.apply(x))
      test::runnable::current().fail("serialization failed: {}",
                                     sink.get_error());
    binary_deserializer source(sys, make_span(buf));
    T y;
    if (!source.apply(y))
      test::runnable::current().fail("serialization failed: {}",
                                     source.get_error());
    return y;
  }
};

#define CHECK_TO_STRING(addr) check_eq(addr, to_string(addr##_ep))

#define CHECK_COMPARISON(addr1, addr2)                                         \
  check_gt(addr2##_ep, addr1##_ep);                                            \
  check_ge(addr2##_ep, addr1##_ep);                                            \
  check_ge(addr1##_ep, addr1##_ep);                                            \
  check_ge(addr2##_ep, addr2##_ep);                                            \
  check_eq(addr1##_ep, addr1##_ep);                                            \
  check_eq(addr2##_ep, addr2##_ep);                                            \
  check_le(addr1##_ep, addr2##_ep);                                            \
  check_le(addr1##_ep, addr1##_ep);                                            \
  check_le(addr2##_ep, addr2##_ep);                                            \
  check_ne(addr1##_ep, addr2##_ep);                                            \
  check_ne(addr2##_ep, addr1##_ep);

#define CHECK_SERIALIZATION(addr) check_eq(addr##_ep, roundtrip(addr##_ep))

WITH_FIXTURE(fixture) {

TEST("constructing assigning and hash_code") {
  const uint16_t port = 8888;
  ipv6_address::array_type bytes{0, 0, 0, 0, 0, 0, 0, 0,
                                 0, 0, 0, 0, 0, 0, 0, 1};
  auto addr = ipv6_address{bytes};
  ipv6_endpoint ep1(addr, port);
  check_eq(ep1.address(), addr);
  check_eq(ep1.port(), port);
  ipv6_endpoint ep2;
  ep2.address(addr);
  ep2.port(port);
  check_eq(ep2.address(), addr);
  check_eq(ep2.port(), port);
  check_eq(ep1, ep2);
  check_eq(ep1.hash_code(), ep2.hash_code());
}

TEST("comparison to IPv4") {
  ipv4_endpoint v4{ipv4_address({127, 0, 0, 1}), 8080};
  ipv6_endpoint v6{v4.address(), v4.port()};
  check_eq(v4, v6);
  check_eq(v6, v4);
}

TEST("to_string") {
  CHECK_TO_STRING("[::1]:8888");
  CHECK_TO_STRING("[4e::d00:0:ed00:0:1]:1234");
  CHECK_TO_STRING("[::1]:1111");
  CHECK_TO_STRING("[4432::33:1]:8732");
  CHECK_TO_STRING("[::2]:8888");
  CHECK_TO_STRING("[4f::d00:12:ed00:0:1]:1234");
  CHECK_TO_STRING("[4f::1]:2222");
  CHECK_TO_STRING("[4432:8d::33:1]:8732");
  CHECK_TO_STRING("[4e::d00:0:ed00:0:1]:5678");
  CHECK_TO_STRING("[::1]:2221");
  CHECK_TO_STRING("[::1]:2222");
  CHECK_TO_STRING("[4432::33:1]:872");
  CHECK_TO_STRING("[4432::33:1]:999");
}

TEST("comparison") {
  CHECK_COMPARISON("[::1]:8888", "[::2]:8888");
  CHECK_COMPARISON("[4e::d00:0:ed00:0:1]:1234", "[4f::d00:12:ed00:0:1]:1234");
  CHECK_COMPARISON("[::1]:1111", "[4f::1]:2222");
  CHECK_COMPARISON("[4432::33:1]:8732", "[4432:8d::33:1]:8732");
  CHECK_COMPARISON("[::1]:1111", "[::1]:8888");
  CHECK_COMPARISON("[4e::d00:0:ed00:0:1]:1234", "[4e::d00:0:ed00:0:1]:5678");
  CHECK_COMPARISON("[::1]:2221", "[::1]:2222");
  CHECK_COMPARISON("[4432::33:1]:872", "[4432::33:1]:999");
}

TEST("serialization") {
  CHECK_SERIALIZATION("[::1]:8888");
  CHECK_SERIALIZATION("[4e::d00:0:ed00:0:1]:1234");
  CHECK_SERIALIZATION("[::1]:1111");
  CHECK_SERIALIZATION("[4432::33:1]:8732");
  CHECK_SERIALIZATION("[::2]:8888");
  CHECK_SERIALIZATION("[4f::d00:12:ed00:0:1]:1234");
  CHECK_SERIALIZATION("[4f::1]:2222");
  CHECK_SERIALIZATION("[4432:8d::33:1]:8732");
  CHECK_SERIALIZATION("[4e::d00:0:ed00:0:1]:5678");
  CHECK_SERIALIZATION("[::1]:2221");
  CHECK_SERIALIZATION("[::1]:2222");
  CHECK_SERIALIZATION("[4432::33:1]:872");
  CHECK_SERIALIZATION("[4432::33:1]:999");
}

} // WITH_FIXTURE(fixture)

} // namespace
