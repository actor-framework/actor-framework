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

struct test_data {
  using comparison_testcase = std::pair<ipv6_endpoint, ipv6_endpoint>;
  using to_string_testcase = std::pair<ipv6_endpoint, std::string>;
  using addr_bytes = ipv6_address::array_type;

  test_data() {
    // different ip but same port
    add(make_ipv6_endpoint({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
                           8888),
        make_ipv6_endpoint({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2},
                           8888));
    add(make_ipv6_endpoint({0, 78, 0, 0, 0, 0, 13, 0, 0, 0, 237, 0, 0, 0, 0, 1},
                           8888),
        make_ipv6_endpoint({1, 0, 0, 0, 9, 0, 0, 0, 0, 27, 0, 0, 0, 0, 0, 2},
                           8888));
    add(make_ipv6_endpoint({0, 78, 0, 0, 0, 255, 13, 0, 0, 0, 237, 0, 0, 0, 0,
                            17},
                           8888),
        make_ipv6_endpoint({1, 0, 0, 0, 9, 0, 0, 0, 0, 27, 0, 0, 0, 255, 0, 3},
                           8888));
    add(make_ipv6_endpoint({0, 78, 0, 0, 0, 0, 13, 0, 0, 0, 237, 0, 0, 0, 0, 1},
                           8888),
        make_ipv6_endpoint({1, 0, 0, 0, 9, 0, 0, 0, 0, 27, 0, 0, 0, 0, 0, 2},
                           8888));
    // same ip but different port
    add(make_ipv6_endpoint({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
                           1111),
        make_ipv6_endpoint({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
                           8888));
    add(make_ipv6_endpoint({0, 78, 0, 0, 0, 0, 13, 0, 0, 0, 237, 0, 0, 0, 0, 1},
                           1234),
        make_ipv6_endpoint({0, 78, 0, 0, 0, 0, 13, 0, 0, 0, 237, 0, 0, 0, 0, 1},
                           5678));
    add(make_ipv6_endpoint({0, 78, 0, 0, 0, 255, 13, 0, 0, 0, 237, 0, 0, 0, 0,
                            17},
                           5678),
        make_ipv6_endpoint({0, 78, 0, 0, 0, 255, 13, 0, 0, 0, 237, 0, 0, 0, 0,
                            17},
                           12345));
    add(make_ipv6_endpoint({0, 78, 0, 0, 0, 0, 13, 0, 0, 0, 237, 0, 0, 0, 0, 1},
                           8888),
        make_ipv6_endpoint({0, 78, 0, 0, 0, 0, 13, 0, 0, 0, 237, 0, 0, 0, 0, 1},
                           9999));
    // String test-data
    add(make_ipv6_endpoint({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
                           1111),
        "[::1]:1111");
    add(make_ipv6_endpoint({0, 78, 0, 0, 0, 0, 13, 0, 0, 0, 237, 0, 0, 0, 0, 1},
                           1234),
        "[4e::d00:0:ed00:0:1]:1234");
    add(make_ipv6_endpoint({0, 78, 0, 0, 0, 255, 13, 0, 0, 0, 237, 0, 0, 0, 0,
                            17},
                           2345),
        "[4e:0:ff:d00:0:ed00:0:11]:2345");
    add(make_ipv6_endpoint({0, 78, 0, 0, 0, 0, 13, 0, 0, 0, 237, 0, 0, 0, 0, 1},
                           1234),
        "[4e::d00:0:ed00:0:1]:1234");
  }

  // First endpoint is always smaller than the second.
  std::vector<comparison_testcase> comparison_testdata;
  std::vector<to_string_testcase> to_string_testdata;

  ipv6_endpoint make_ipv6_endpoint(addr_bytes bytes, uint16_t port) {
    return ipv6_endpoint{ipv6_address{bytes}, port};
  }

private:
  // Convenience functions for adding testcases.
  void add(ipv6_endpoint ep1, ipv6_endpoint ep2) {
    comparison_testdata.emplace_back(comparison_testcase{ep1, ep2});
  }

  void add(ipv6_endpoint ep, const std::string& str) {
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
    ipv6_endpoint deserialized_data;
    if (auto err = source(deserialized_data))
      CAF_FAIL("deserialization failed: " << sys.render(err));
    CAF_CHECK_EQUAL(testcase.first, deserialized_data);
  }
}

CAF_TEST_FIXTURE_SCOPE_END()