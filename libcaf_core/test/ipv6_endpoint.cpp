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

#define CAF_SUITE ipv6_endpoint

#include "caf/ipv6_endpoint.hpp"

#include "caf/test/unit_test.hpp"

#include <cassert>
#include <vector>

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/byte.hpp"
#include "caf/ipv6_address.hpp"
#include "caf/serializer_impl.hpp"
#include "caf/span.hpp"

using namespace caf;

namespace {

ipv6_endpoint operator"" _ep(const char* str, size_t) {
  union {
    std::array<uint8_t, 16> xs_bytes;
    std::array<uint16_t, 8> xs;
  };
  union {
    std::array<uint8_t, 16> ys_bytes;
    std::array<uint16_t, 8> ys;
  };
  memset(xs_bytes.data(), 0, xs_bytes.size());
  memset(ys_bytes.data(), 0, xs_bytes.size());
  assert(*str == '[');
  ++str;
  char* next = nullptr;
  auto pull = [&] {
    auto x = static_cast<uint16_t>(strtol(str, &next, 16));
    return detail::to_network_order(x);
  };
  size_t n = 0;
  do {
    xs[n++] = pull();
    assert(next != nullptr);
    str = next + 1;
  } while (*str != ':' && *str != ']');
  if (*str == ':') {
    ++str;
    do {
      ys[0] = pull();
      std::rotate(ys.begin(), ys.begin() + 1, ys.end());
      assert(next != nullptr);
      str = next + 1;
    } while (*next != ']');
  }
  assert(*next == ']');
  assert(*str == ':');
  ++str;
  auto port = static_cast<uint16_t>(strtol(str, nullptr, 10));
  std::array<uint8_t, 16> bytes;
  for (size_t i = 0; i < 16; ++i)
    bytes[i] = xs_bytes[i] | ys_bytes[i];
  return ipv6_endpoint{ipv6_address{bytes}, port};
}

struct fixture {
  actor_system_config cfg;
  actor_system sys{cfg};

  template <class T>
  T roundtrip(T x) {
    using container_type = std::vector<byte>;
    container_type buf;
    serializer_impl<container_type> sink(sys, buf);
    if (auto err = sink(x))
      CAF_FAIL("serialization failed: " << sys.render(err));
    binary_deserializer source(sys, make_span(buf));
    T y;
    if (auto err = source(y))
      CAF_FAIL("deserialization failed: " << sys.render(err));
    return y;
  }
};

#define CHECK_TO_STRING(addr) CAF_CHECK_EQUAL(addr, to_string(addr##_ep))

#define CHECK_COMPARISON(addr1, addr2)                                         \
  CAF_CHECK_GREATER(addr2##_ep, addr1##_ep);                                   \
  CAF_CHECK_GREATER_OR_EQUAL(addr2##_ep, addr1##_ep);                          \
  CAF_CHECK_GREATER_OR_EQUAL(addr1##_ep, addr1##_ep);                          \
  CAF_CHECK_GREATER_OR_EQUAL(addr2##_ep, addr2##_ep);                          \
  CAF_CHECK_EQUAL(addr1##_ep, addr1##_ep);                                     \
  CAF_CHECK_EQUAL(addr2##_ep, addr2##_ep);                                     \
  CAF_CHECK_LESS_OR_EQUAL(addr1##_ep, addr2##_ep);                             \
  CAF_CHECK_LESS_OR_EQUAL(addr1##_ep, addr1##_ep);                             \
  CAF_CHECK_LESS_OR_EQUAL(addr2##_ep, addr2##_ep);                             \
  CAF_CHECK_NOT_EQUAL(addr1##_ep, addr2##_ep);                                 \
  CAF_CHECK_NOT_EQUAL(addr2##_ep, addr1##_ep);

#define CHECK_SERIALIZATION(addr)                                              \
  CAF_CHECK_EQUAL(addr##_ep, roundtrip(addr##_ep))

} // namespace

CAF_TEST_FIXTURE_SCOPE(comparison_scope, fixture)

CAF_TEST(constructing assigning and hash_code) {
  const uint16_t port = 8888;
  ipv6_address::array_type bytes{0, 0, 0, 0, 0, 0, 0, 0,
                                 0, 0, 0, 0, 0, 0, 0, 1};
  auto addr = ipv6_address{bytes};
  ipv6_endpoint ep1(addr, port);
  CAF_CHECK_EQUAL(ep1.address(), addr);
  CAF_CHECK_EQUAL(ep1.port(), port);
  ipv6_endpoint ep2;
  ep2.address(addr);
  ep2.port(port);
  CAF_CHECK_EQUAL(ep2.address(), addr);
  CAF_CHECK_EQUAL(ep2.port(), port);
  CAF_CHECK_EQUAL(ep1, ep2);
  CAF_CHECK_EQUAL(ep1.hash_code(), ep2.hash_code());
}

CAF_TEST(to_string) {
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

CAF_TEST(comparison) {
  CHECK_COMPARISON("[::1]:8888", "[::2]:8888");
  CHECK_COMPARISON("[4e::d00:0:ed00:0:1]:1234", "[4f::d00:12:ed00:0:1]:1234");
  CHECK_COMPARISON("[::1]:1111", "[4f::1]:2222");
  CHECK_COMPARISON("[4432::33:1]:8732", "[4432:8d::33:1]:8732");
  CHECK_COMPARISON("[::1]:1111", "[::1]:8888");
  CHECK_COMPARISON("[4e::d00:0:ed00:0:1]:1234", "[4e::d00:0:ed00:0:1]:5678");
  CHECK_COMPARISON("[::1]:2221", "[::1]:2222");
  CHECK_COMPARISON("[4432::33:1]:872", "[4432::33:1]:999");
}

CAF_TEST(serialization) {
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

CAF_TEST_FIXTURE_SCOPE_END()