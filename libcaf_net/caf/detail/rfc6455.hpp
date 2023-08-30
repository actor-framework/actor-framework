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

  enum class opcode_type : uint8_t {
    continuation_frame = 0x00,
    text_frame = 0x01,
    binary_frame = 0x02,
    connection_close_frame = 0x08,
    ping_frame = 0x09,
    pong_frame = 0x0A,
    /// Invalid opcode to mean "no opcode received yet".
    nil_code = 0xFF,
  };

  struct header {
    bool fin = false;
    opcode_type opcode = opcode_type::nil_code;
    uint32_t mask_key = 0;
    uint64_t payload_len = 0;

    constexpr bool valid() const noexcept {
      return opcode != opcode_type::nil_code;
    }
  };

  // -- constants --------------------------------------------------------------

  static constexpr uint8_t fin_flag = 0x80;

  // -- utility functions ------------------------------------------------------

  static void mask_data(uint32_t key, span<char> data, size_t offset = 0);

  static void mask_data(uint32_t key, byte_span data, size_t offset = 0);

  static void assemble_frame(uint32_t mask_key, span<const char> data,
                             byte_buffer& out);

  static void assemble_frame(uint32_t mask_key, const_byte_span data,
                             byte_buffer& out);

  static void assemble_frame(opcode_type opcode, uint32_t mask_key,
                             const_byte_span data, byte_buffer& out,
                             uint8_t flags = fin_flag);

  static ptrdiff_t decode_header(const_byte_span data, header& hdr);

  static constexpr bool is_control_frame(opcode_type opcode) noexcept {
    return opcode > opcode_type::binary_frame;
  }
};

} // namespace caf::detail
