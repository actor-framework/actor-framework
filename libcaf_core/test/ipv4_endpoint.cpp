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
 * (at your option) under th#e terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#define CAF_SUITE ipv4_endpoint

#include "caf/ipv4_endpoint.hpp"

#include "caf/test/unit_test.hpp"

#include <vector>

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/byte.hpp"
#include "caf/ipv4_address.hpp"
#include "caf/serializer_impl.hpp"
#include "caf/span.hpp"

using namespace caf;

namespace {

struct test_data {
  using endpoint = ipv4_endpoint;
  using comparison_testcase = std::pair<endpoint, endpoint>;
  using to_string_testcase = std::pair<endpoint, std::string>;

  test_data() {
    const auto addr = make_ipv4_address;
    // different ip but same port
    add({addr(127, 0, 0, 1), 8888}, {addr(127, 0, 0, 2), 8888});
    add({addr(192, 168, 178, 1), 8888}, {addr(245, 114, 2, 89), 8888});
    add({addr(188, 56, 23, 97), 1211}, {addr(189, 22, 36, 0), 1211});
    add({addr(0, 0, 0, 0), 8888}, {addr(255, 255, 255, 1), 8888});
    // same ip but different port
    add({addr(127, 0, 0, 1), 111}, {addr(127, 0, 0, 1), 8888});
    add({addr(192, 168, 178, 1), 8888}, {addr(245, 114, 2, 89), 8888});
    add({addr(127, 0, 0, 1), 111}, {addr(127, 0, 0, 1), 8888});
    add({addr(123, 123, 123, 123), 8888}, {addr(123, 123, 123, 123), 8889});

    add({addr(127, 0, 0, 1), 8888}, "127.0.0.1:8888");
    add({addr(192, 168, 178, 1), 8888}, "192.168.178.1:8888");
    add({addr(127, 0, 0, 1), 111}, "127.0.0.1:111");
    add({addr(192, 168, 178, 1), 8888}, "192.168.178.1:8888");
    add({addr(127, 0, 0, 1), 111}, "127.0.0.1:111");
    add({addr(123, 123, 123, 123), 8888}, "123.123.123.123:8888");
  }

  // First endpoint is always smaller than the second.
  std::vector<comparison_testcase> comparison_testdata;
  std::vector<to_string_testcase> to_string_testdata;

private:
  void add(endpoint ep1, endpoint ep2) {
    comparison_testdata.emplace_back(comparison_testcase{ep1, ep2});
  }

  void add(endpoint ep, const std::string& str) {
    to_string_testdata.emplace_back(to_string_testcase{ep, str});
  }
};

struct test_fixture : public test_data {
  actor_system_config cfg;
  actor_system sys{cfg};
};

} // namespace

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

CAF_TEST_FIXTURE_SCOPE(comparison_scope, test_fixture)

CAF_TEST(to_string) {
  for (auto& testcase : to_string_testdata)
    CAF_CHECK_EQUAL(to_string(testcase.first), testcase.second);
}

CAF_TEST(comparison) {
  for (auto testcase : comparison_testdata) {
    // First member of this pair is always smaller than the second one.
    auto ep1 = testcase.first;
    auto ep2 = testcase.second;
    CAF_CHECK_GREATER(ep2, ep1);
    CAF_CHECK_GREATER_OR_EQUAL(ep2, ep1);
    CAF_CHECK_GREATER_OR_EQUAL(ep1, ep1);
    CAF_CHECK_GREATER_OR_EQUAL(ep2, ep2);
    CAF_CHECK_EQUAL(ep1, ep1);
    CAF_CHECK_EQUAL(ep2, ep2);
    CAF_CHECK_LESS_OR_EQUAL(ep1, ep2);
    CAF_CHECK_LESS_OR_EQUAL(ep1, ep1);
    CAF_CHECK_LESS_OR_EQUAL(ep2, ep2);
    CAF_CHECK_NOT_EQUAL(ep1, ep2);
    CAF_CHECK_NOT_EQUAL(ep2, ep1);
  }
}

CAF_TEST(serialization) {
  using container_type = std::vector<byte>;
  for (auto& testcase : comparison_testdata) {
    container_type buf;
    serializer_impl<container_type> sink(sys, buf);
    if (auto err = sink(testcase.first))
      CAF_FAIL("serialization failed: " << sys.render(err));
    binary_deserializer source(sys, make_span(buf));
    ipv4_endpoint deserialized_data;
    if (auto err = source(deserialized_data))
      CAF_FAIL("deserialization failed: " << sys.render(err));
    CAF_CHECK_EQUAL(testcase.first, deserialized_data);
  }
}

CAF_TEST_FIXTURE_SCOPE_END()