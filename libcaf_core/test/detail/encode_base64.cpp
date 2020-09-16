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
