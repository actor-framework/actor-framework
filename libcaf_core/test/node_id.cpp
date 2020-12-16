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

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"

using namespace caf;

namespace {

node_id roundtrip(node_id nid) {
  byte_buffer buf;
  {
    binary_serializer sink{nullptr, buf};
    if (!sink.apply(nid))
      CAF_FAIL("serialization failed: " << sink.get_error());
  }
  if (buf.empty())
    CAF_FAIL("serializer produced no output");
  node_id result;
  {
    binary_deserializer source{nullptr, buf};
    if (!source.apply(result))
      CAF_FAIL("deserialization failed: " << source.get_error());
    if (source.remaining() > 0)
      CAF_FAIL("binary_serializer ignored part of its input");
  }
  return result;
}

} // namespace

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

CAF_TEST(node IDs reject malformed strings) {
  // not URIs
  CHECK_PARSE_FAIL("foobar");
  CHECK_PARSE_FAIL("CAF#1");
  // uint32_t overflow on the process ID
  CHECK_PARSE_FAIL("0102030405060708090A0B0C0D0E0F1011121314#42949672950");
}

CAF_TEST(node IDs are serializable) {
  CAF_MESSAGE("empty node IDs remain empty");
  {
    node_id nil_id;
    CAF_CHECK_EQUAL(nil_id, roundtrip(nil_id));
  }
  CAF_MESSAGE("hash-based node IDs remain intact");
  {
    auto tmp = make_node_id(42, "0102030405060708090A0B0C0D0E0F1011121314");
    auto hash_based_id = unbox(tmp);
    CAF_CHECK_EQUAL(hash_based_id, roundtrip(hash_based_id));
  }
  CAF_MESSAGE("URI-based node IDs remain intact");
  {
    auto uri_based_id = make_node_id(unbox(make_uri("foo:bar")));
    CAF_CHECK_EQUAL(uri_based_id, roundtrip(uri_based_id));
  }
}
