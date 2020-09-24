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

#pragma once

#include "caf/byte.hpp"
#include "caf/detail/net_export.hpp"

#include <cstdint>
#include <vector>

namespace caf::detail {

struct CAF_NET_EXPORT rfc6455 {
  // -- member types -----------------------------------------------------------

  using binary_buffer = std::vector<byte>;

  struct header {
    bool fin;
    uint8_t opcode;
    uint32_t mask_key;
    uint64_t payload_len;
  };

  // -- constants --------------------------------------------------------------

  static constexpr uint8_t continuation_frame = 0x00;

  static constexpr uint8_t text_frame = 0x01;

  static constexpr uint8_t binary_frame = 0x02;

  static constexpr uint8_t connection_close = 0x08;

  static constexpr uint8_t ping = 0x09;

  static constexpr uint8_t pong = 0x0A;

  // -- utility functions ------------------------------------------------------

  static void mask_data(uint32_t key, span<char> data);

  static void mask_data(uint32_t key, span<byte> data);

  static void assemble_frame(uint32_t mask_key, span<const char> data,
                             binary_buffer& out);

  static void assemble_frame(uint32_t mask_key, span<const byte> data,
                             binary_buffer& out);

  static void assemble_frame(uint8_t opcode, uint32_t mask_key,
                             span<const byte> data, binary_buffer& out);

  static ptrdiff_t decode_header(span<const byte> data, header& hdr);
};

} // namespace caf::detail
