// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE basp.header

#include "caf/net/basp/header.hpp"

#include "caf/test/dsl.hpp"

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
    CAF_CHECK(sink.apply_object(x));
  }
  CAF_CHECK_EQUAL(buf.size(), basp::header_size);
  auto buf2 = to_bytes(x);
  CAF_REQUIRE_EQUAL(buf.size(), buf2.size());
  CAF_CHECK(std::equal(buf.begin(), buf.end(), buf2.begin()));
  basp::header y;
  {
    binary_deserializer source{nullptr, buf};
    CAF_CHECK(source.apply_object(y));
  }
  CAF_CHECK_EQUAL(x, y);
  auto z = basp::header::from_bytes(buf);
  CAF_CHECK_EQUAL(x, z);
  CAF_CHECK_EQUAL(y, z);
}

CAF_TEST(to_string) {
  basp::header x{basp::message_type::handshake, 42, 4};
  CAF_CHECK_EQUAL(deep_to_string(x), "basp::header(handshake, 42, 4)");
}
