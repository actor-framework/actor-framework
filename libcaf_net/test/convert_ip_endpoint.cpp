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

#define CAF_SUITE convert_ip_endpoint

#include "caf/detail/convert_ip_endpoint.hpp"

#include "caf/test/dsl.hpp"

#include <cstring>

#include "caf/detail/socket_sys_includes.hpp"

using namespace caf;
using namespace caf::detail;

CAF_TEST(sockaddr roundtrip) {
  sockaddr_in6 source_addr = {};
  source_addr.sin6_family = AF_INET6;
  source_addr.sin6_port = htons(23);
  source_addr.sin6_addr = in6addr_loopback;
  auto ep = to_ip_endpoint(source_addr);
  auto dest_addr = to_sockaddr(ep);
  CAF_CHECK_EQUAL(memcmp(&source_addr, &dest_addr, sizeof(sockaddr_in6)), 0);
}

CAF_TEST(ip_endpoint roundtrip) {
  ip_endpoint source_ep;
  if (auto err = detail::parse("[::1]:55555", source_ep))
    CAF_FAIL("unable to parse input: " << err);
  auto addr = to_sockaddr(source_ep);
  auto dest_ep = to_ip_endpoint(addr);
  CAF_CHECK_EQUAL(source_ep, dest_ep);
}
