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

#define CAF_SUITE hash.sha1

#include "caf/hash/sha1.hpp"

#include "caf/test/dsl.hpp"

using namespace caf;

namespace {

template <class... Ts>
auto make_hash(Ts... xs) {
  static_assert(sizeof...(Ts) == 20);
  return hash::sha1::result_type{{static_cast<byte>(xs)...}};
}

} // namespace

#define CHECK_HASH_EQ(str, bytes)                                              \
  CAF_CHECK_EQUAL(hash::sha1::compute(string_view{str}), bytes);

CAF_TEST(strings are hashed by their content only) {
  CHECK_HASH_EQ("dGhlIHNhbXBsZSBub25jZQ==258EAFA5-E914-47DA-95CA-C5AB0DC85B11",
                make_hash(0xb3, 0x7a, 0x4f, 0x2c,   // Bytes  1 -  4
                          0xc0, 0x62, 0x4f, 0x16,   //        5 -  8
                          0x90, 0xf6, 0x46, 0x06,   //        9 - 12
                          0xcf, 0x38, 0x59, 0x45,   //       13 - 16
                          0xb2, 0xbe, 0xc4, 0xea)); //       17 - 20
}
