// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/node_id.hpp"

#include "caf/test/test.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/log/test.hpp"

using namespace caf;

namespace {

node_id roundtrip(node_id nid) {
  auto& this_test = test::runnable::current();
  byte_buffer buf;
  {
    binary_serializer sink{nullptr, buf};
    if (!sink.apply(nid))
      this_test.fail("serialization failed: {}", sink.get_error());
  }
  if (buf.empty())
    this_test.fail("serializer produced no output");
  node_id result;
  {
    binary_deserializer source{nullptr, buf};
    if (!source.apply(result))
      this_test.fail("deserialization failed: {}", source.get_error());
    if (source.remaining() > 0)
      this_test.fail("binary_serializer ignored part of its input");
  }
  return result;
}

template <class T>
T unbox(caf::expected<T> x) {
  if (!x)
    test::runnable::current().fail("{}", to_string(x.error()));
  return std::move(*x);
}

template <class T>
T unbox(std::optional<T> x) {
  if (!x)
    test::runnable::current().fail("x == std::nullopt");
  return std::move(*x);
}

#define CHECK_PARSE_OK(str, ...)                                               \
  do {                                                                         \
    check(node_id::can_parse(str));                                            \
    node_id nid;                                                               \
    check_eq(parse(str, nid), none);                                           \
    check_eq(nid, make_node_id(__VA_ARGS__));                                  \
  } while (false)

TEST("node IDs are convertible from string") {
  node_id::default_data::host_id_type hash{{
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
  }};
  auto uri_id = unbox(make_uri("ip://foo:8080"));
  CHECK_PARSE_OK("0102030405060708090A0B0C0D0E0F1011121314#1", 1, hash);
  CHECK_PARSE_OK("0102030405060708090A0B0C0D0E0F1011121314#123", 123, hash);
  CHECK_PARSE_OK("ip://foo:8080", uri_id);
}

#define CHECK_PARSE_FAIL(str) check(!node_id::can_parse(str))

TEST("node IDs reject malformed strings") {
  // not URIs
  CHECK_PARSE_FAIL("foobar");
  CHECK_PARSE_FAIL("CAF#1");
  // uint32_t overflow on the process ID
  CHECK_PARSE_FAIL("0102030405060708090A0B0C0D0E0F1011121314#42949672950");
}

TEST("node IDs are serializable") {
  log::test::debug("empty node IDs remain empty");
  {
    node_id nil_id;
    check_eq(nil_id, roundtrip(nil_id));
  }
  log::test::debug("hash-based node IDs remain intact");
  {
    auto tmp = make_node_id(42, "0102030405060708090A0B0C0D0E0F1011121314");
    auto hash_based_id = unbox(tmp);
    check_eq(hash_based_id, roundtrip(hash_based_id));
  }
  log::test::debug("URI-based node IDs remain intact");
  {
    auto uri_based_id = make_node_id(unbox(make_uri("foo:bar")));
    check_eq(uri_based_id, roundtrip(uri_based_id));
  }
}

} // namespace
