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

#define CAF_SUITE ipv4_endpoint

#include "caf/ipv4_endpoint.hpp"

#include "core-test.hpp"

#include <cassert>
#include <vector>

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/byte.hpp"
#include "caf/byte_buffer.hpp"
#include "caf/detail/parse.hpp"
#include "caf/ipv4_address.hpp"
#include "caf/span.hpp"

using namespace caf;

namespace {

ipv4_endpoint operator"" _ep(const char* str, size_t size) {
  ipv4_endpoint result;
  if (auto err = detail::parse(string_view{str, size}, result))
    CAF_FAIL("unable to parse input: " << err);
  return result;
}

struct fixture {
  actor_system_config cfg;
  actor_system sys{cfg};

  template <class T>
  T roundtrip(T x) {
    byte_buffer buf;
    binary_serializer sink(sys, buf);
    if (auto err = sink(x))
      CAF_FAIL("serialization failed: " << err);
    binary_deserializer source(sys, make_span(buf));
    T y;
    if (auto err = source(y))
      CAF_FAIL("deserialization failed: " << err);
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

CAF_TEST_FIXTURE_SCOPE(ipv4_endpoint_tests, fixture)

CAF_TEST(constructing assigning and hash_code) {
  const uint16_t port = 8888;
  auto addr = make_ipv4_address(127, 0, 0, 1);
  ipv4_endpoint ep1(addr, port);
  CAF_CHECK_EQUAL(ep1.address(), addr);
  CAF_CHECK_EQUAL(ep1.port(), port);
  ipv4_endpoint ep2;
  ep2.address(addr);
  ep2.port(port);
  CAF_CHECK_EQUAL(ep2.address(), addr);
  CAF_CHECK_EQUAL(ep2.port(), port);
  CAF_CHECK_EQUAL(ep1, ep2);
  CAF_CHECK_EQUAL(ep1.hash_code(), ep2.hash_code());
}

CAF_TEST(to string) {
  CHECK_TO_STRING("127.0.0.1:8888");
  CHECK_TO_STRING("192.168.178.1:8888");
  CHECK_TO_STRING("255.255.255.1:17");
  CHECK_TO_STRING("192.168.178.1:8888");
  CHECK_TO_STRING("127.0.0.1:111");
  CHECK_TO_STRING("123.123.123.123:8888");
  CHECK_TO_STRING("127.0.0.1:8888");
}

CAF_TEST(comparison) {
  CHECK_COMPARISON("127.0.0.1:8888", "127.0.0.2:8888");
  CHECK_COMPARISON("192.168.178.1:8888", "245.114.2.89:8888");
  CHECK_COMPARISON("188.56.23.97:1211", "189.22.36.0:1211");
  CHECK_COMPARISON("0.0.0.0:8888", "255.255.255.1:8888");
  CHECK_COMPARISON("127.0.0.1:111", "127.0.0.1:8888");
  CHECK_COMPARISON("192.168.178.1:8888", "245.114.2.89:8888");
  CHECK_COMPARISON("123.123.123.123:8888", "123.123.123.123:8889");
}

CAF_TEST(serialization) {
  CHECK_SERIALIZATION("127.0.0.1:8888");
  CHECK_SERIALIZATION("192.168.178.1:8888");
  CHECK_SERIALIZATION("255.255.255.1:17");
  CHECK_SERIALIZATION("192.168.178.1:8888");
  CHECK_SERIALIZATION("127.0.0.1:111");
  CHECK_SERIALIZATION("123.123.123.123:8888");
  CHECK_SERIALIZATION("127.0.0.1:8888");
}

CAF_TEST_FIXTURE_SCOPE_END()
