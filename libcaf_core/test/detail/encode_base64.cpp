// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE detail.encode_base64

#include "caf/detail/encode_base64.hpp"

#include "caf/test/dsl.hpp"

#include <array>

using namespace caf;

namespace {

template <class... Ts>
auto encode(Ts... xs) {
  std::array<byte, sizeof...(Ts)> bytes{{static_cast<byte>(xs)...}};
  return detail::encode_base64(bytes);
}

} // namespace

CAF_TEST(base64 encoding converts byte sequences to strings) {
  CAF_CHECK_EQUAL(encode(0xb3, 0x7a, 0x4f, 0x2c, 0xc0, 0x62, 0x4f, 0x16, 0x90,
                         0xf6, 0x46, 0x06, 0xcf, 0x38, 0x59, 0x45, 0xb2, 0xbe,
                         0xc4, 0xea),
                  "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
}
