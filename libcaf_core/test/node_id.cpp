/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#define CAF_SUITE node_id

#include "caf/node_id.hpp"

#include "caf/test/dsl.hpp"

using namespace caf;

#define CHECK_PARSE_OK(str, ...)                                               \
  do {                                                                         \
    CAF_CHECK(node_id::can_parse(str));                                        \
    node_id nid;                                                               \
    CAF_CHECK_EQUAL(parse(str, nid), none);                                    \
    CAF_CHECK_EQUAL(nid, make_node_id(__VA_ARGS__));                           \
  } while (false)

CAF_TEST(node IDs are convertible from string) {
  node_id::default_data::host_id_type hash{{
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
  }};
  auto uri_id = unbox(make_uri("ip://foo:8080"));
  CHECK_PARSE_OK("0102030405060708090A0B0C0D0E0F1011121314#1", 1, hash);
  CHECK_PARSE_OK("0102030405060708090A0B0C0D0E0F1011121314#123", 123, hash);
  CHECK_PARSE_OK("ip://foo:8080", uri_id);
}

#define CHECK_PARSE_FAIL(str) CAF_CHECK(!node_id::can_parse(str))

CAF_TEST(node IDs are not convertible from malformed strings) {
  // not URIs
  CHECK_PARSE_FAIL("foobar");
  CHECK_PARSE_FAIL("CAF#1");
  // uint32_t overflow on the process ID
  CHECK_PARSE_FAIL("0102030405060708090A0B0C0D0E0F1011121314#42949672950");
}
