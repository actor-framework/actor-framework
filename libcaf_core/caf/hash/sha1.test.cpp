// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/hash/sha1.hpp"

#include "caf/test/test.hpp"

using namespace std::literals;

using caf::hash::sha1;

namespace {

template <class... Ts>
auto make_bytes(Ts... xs) {
  static_assert(sizeof...(Ts) == 20);
  return sha1::result_type{{static_cast<std::byte>(xs)...}};
}

TEST("strings are hashed by their content only") {
  auto str = "dGhlIHNhbXBsZSBub25jZQ==258EAFA5-E914-47DA-95CA-C5AB0DC85B11"sv;
  check_eq(sha1::compute(str),
           make_bytes(0xb3, 0x7a, 0x4f, 0x2c,   // Bytes  1 -  4
                      0xc0, 0x62, 0x4f, 0x16,   //        5 -  8
                      0x90, 0xf6, 0x46, 0x06,   //        9 - 12
                      0xcf, 0x38, 0x59, 0x45,   //       13 - 16
                      0xb2, 0xbe, 0xc4, 0xea)); //       17 - 20
}

} // namespace
