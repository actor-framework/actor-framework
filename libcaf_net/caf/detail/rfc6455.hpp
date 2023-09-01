// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/byte_buffer.hpp"
#include "caf/byte_span.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"
#include "caf/span.hpp"

#include <cstdint>

namespace caf::detail {

struct CAF_NET_EXPORT rfc6455 {
  // -- member types -----------------------------------------------------------

  struct header {
    bool fin = false;
    uint8_t opcode = invalid_frame;
    uint32_t mask_key = 0;
    uint64_t payload_len = 0;

    constexpr bool valid() const noexcept {
      return opcode != invalid_frame;
    }
  };

  // -- constants --------------------------------------------------------------

  static constexpr uint8_t continuation_frame = 0x00;

  static constexpr uint8_t text_frame = 0x01;

  static constexpr uint8_t binary_frame = 0x02;

  static constexpr uint8_t connection_close_frame = 0x08;

  static constexpr uint8_t ping_frame = 0x09;

  static constexpr uint8_t pong_frame = 0x0A;

  /// Invalid opcode to mean "no opcode received yet".
  static constexpr uint8_t invalid_frame = 0xFF;

  static constexpr uint8_t fin_flag = 0x80;

  // -- utility functions ------------------------------------------------------

  static void mask_data(uint32_t key, span<char> data, size_t offset = 0);

  static void mask_data(uint32_t key, byte_span data, size_t offset = 0);

  static void assemble_frame(uint32_t mask_key, span<const char> data,
                             byte_buffer& out);

  static void assemble_frame(uint32_t mask_key, const_byte_span data,
                             byte_buffer& out);

  static void assemble_frame(uint8_t opcode, uint32_t mask_key,
                             const_byte_span data, byte_buffer& out,
                             uint8_t flags = fin_flag);

  static ptrdiff_t decode_header(const_byte_span data, header& result);

  static constexpr bool is_control_frame(uint8_t opcode) noexcept {
    return opcode > binary_frame;
  }
};

} // namespace caf::detail
