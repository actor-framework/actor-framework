// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

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
