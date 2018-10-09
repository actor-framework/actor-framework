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

#define CAF_SUITE ip_endpoint

#include "caf/ip_endpoint.hpp"

#include "caf/test/dsl.hpp"

#include "caf/ipv4_address.hpp"

using namespace caf;

CAF_TEST(construction and comparison) {
  std::hash<ip_endpoint> hash;
  ip_endpoint a;
  CAF_CHECK_EQUAL(a.address(), ip_address());
  CAF_CHECK_EQUAL(a.port(), 0u);
  ip_endpoint b{make_ipv4_address(127, 0, 0, 1), 8080, protocol::tcp};
  CAF_CHECK_EQUAL(b.address(), make_ipv4_address(127, 0, 0, 1));
  CAF_CHECK_EQUAL(b.port(), 8080u);
  CAF_CHECK_EQUAL(b.transport(), protocol::tcp);
  CAF_CHECK_NOT_EQUAL(a, b);
  CAF_CHECK_NOT_EQUAL(hash(a), hash(b));
  ip_endpoint c{b};
  CAF_CHECK_EQUAL(b, c);
  CAF_CHECK_EQUAL(hash(b), hash(c));
  ip_endpoint d{make_ipv4_address(127, 0, 0, 1), 8080, protocol::udp};
  CAF_CHECK_NOT_EQUAL(c, d);
  CAF_CHECK_NOT_EQUAL(hash(c), hash(d));
}
