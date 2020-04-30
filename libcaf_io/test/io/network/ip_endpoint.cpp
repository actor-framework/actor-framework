/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#define CAF_SUITE io.network.ip_endpoint

#include "caf/io/network/ip_endpoint.hpp"

#include "caf/test/dsl.hpp"

#include <vector>

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/io/middleman.hpp"
#include "caf/io/network/interfaces.hpp"

using namespace caf;
using namespace caf::io;

namespace {

class config : public actor_system_config {
public:
  config() {
    // this will call WSAStartup for network initialization on Windows
    load<io::middleman>();
  }
};

struct fixture : test_coordinator_fixture<> {
  template <class T, class... Ts>
  auto serialize(T& x, Ts&... xs) {
    byte_buffer buf;
    binary_serializer sink{sys, buf};
    if (auto err = sink(x, xs...))
      CAF_FAIL("serialization failed: " << to_string(err));
    return buf;
  }

  template <class Buffer, class T, class... Ts>
  void deserialize(const Buffer& buf, T& x, Ts&... xs) {
    binary_deserializer source{sys, buf};
    if (auto err = source(x, xs...))
      CAF_FAIL("serialization failed: " << to_string(err));
  }
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(ep_endpoint_tests, fixture)

CAF_TEST_DISABLED(ip_endpoint) {
  // create an empty endpoint
  network::ip_endpoint ep;
  ep.clear();
  CAF_CHECK_EQUAL("", network::host(ep));
  CAF_CHECK_EQUAL(uint16_t{0}, network::port(ep));
  CAF_CHECK_EQUAL(size_t{0}, *ep.length());
  // fill it with data from a local endpoint
  network::interfaces::get_endpoint("localhost", 12345, ep);
  // save the data
  auto h = network::host(ep);
  auto p = network::port(ep);
  auto l = *ep.length();
  CAF_CHECK("localhost" == h || "127.0.0.1" == h || "::1" == h);
  CAF_CHECK_EQUAL(12345, p);
  CAF_CHECK(0 < l);
  // serialize the endpoint and clear it
  auto buf = serialize(ep);
  auto save = ep;
  ep.clear();
  CAF_CHECK_EQUAL("", network::host(ep));
  CAF_CHECK_EQUAL(uint16_t{0}, network::port(ep));
  CAF_CHECK_EQUAL(size_t{0}, *ep.length());
  // deserialize the data and check if it was load successfully
  deserialize(buf, ep);
  CAF_CHECK_EQUAL(h, network::host(ep));
  CAF_CHECK_EQUAL(uint16_t{p}, network::port(ep));
  CAF_CHECK_EQUAL(size_t{l}, *ep.length());
  CAF_CHECK_EQUAL(save, ep);
}

CAF_TEST_FIXTURE_SCOPE_END()
