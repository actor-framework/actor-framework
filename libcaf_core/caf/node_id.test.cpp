// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

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
    binary_serializer sink{buf};
    if (!sink.apply(nid))
      this_test.fail("serialization failed: {}", sink.get_error());
  }
  if (buf.empty())
    this_test.fail("serializer produced no output");
  node_id result;
  {
    binary_deserializer source{buf};
    if (!source.apply(result))
      this_test.fail("deserialization failed: {}", source.get_error());
    if (source.remaining() > 0)
      this_test.fail("binary_serializer ignored part of its input");
  }
  return result;
}

} // namespace

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
