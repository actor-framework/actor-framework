// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE basp.header

#include "caf/net/basp/header.hpp"

#include "net-test.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/byte_buffer.hpp"
#include "caf/deep_to_string.hpp"

using namespace caf;
using namespace caf::net;

CAF_TEST(serialization) {
  basp::header x{basp::message_type::handshake, 42, 4};
  byte_buffer buf;
  {
    binary_serializer sink{nullptr, buf};
    CHECK(sink.apply(x));
  }
  CHECK_EQ(buf.size(), basp::header_size);
  auto buf2 = to_bytes(x);
  REQUIRE_EQ(buf.size(), buf2.size());
  CHECK(std::equal(buf.begin(), buf.end(), buf2.begin()));
  basp::header y;
  {
    binary_deserializer source{nullptr, buf};
    CHECK(source.apply(y));
  }
  CHECK_EQ(x, y);
  auto z = basp::header::from_bytes(buf);
  CHECK_EQ(x, z);
  CHECK_EQ(y, z);
}

CAF_TEST(to_string) {
  basp::header x{basp::message_type::handshake, 42, 4};
  CHECK_EQ(
    deep_to_string(x),
    "caf::net::basp::header(caf::net::basp::message_type::handshake, 42, 4)");
}
