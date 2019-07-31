/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#define CAF_SUITE basp.header

#include "caf/net/basp/header.hpp"

#include "caf/test/dsl.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/serializer_impl.hpp"

using namespace caf;
using namespace caf::net;

CAF_TEST(serialization) {
  basp::header x{basp::message_type::server_handshake, 42, 4};
  std::vector<byte> buf;
  {
    serializer_impl<std::vector<byte>> sink{nullptr, buf};
    CAF_CHECK_EQUAL(sink(x), none);
  }
  CAF_CHECK_EQUAL(buf.size(), basp::header_size);
  basp::header y;
  {
    binary_deserializer source{nullptr, buf};
    CAF_CHECK_EQUAL(source(y), none);
  }
  CAF_CHECK_EQUAL(x, y);
}

CAF_TEST(to_string) {
  basp::header x{basp::message_type::server_handshake, 42, 4};
  CAF_CHECK_EQUAL(to_string(x), "basp::header(server_handshake, 42, 4)");
}
