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

#include "caf/detail/rfc6455.hpp"

#include "caf/detail/network_order.hpp"
#include "caf/span.hpp"

#include <cstring>

namespace caf::detail {

void rfc6455::mask_data(uint32_t key, span<char> data) {
  mask_data(key, as_writable_bytes(data));
}

void rfc6455::mask_data(uint32_t key, span<byte> data) {
  auto no_key = to_network_order(key);
  byte arr[4];
  memcpy(arr, &no_key, 4);
  size_t i = 0;
  for (auto& x : data) {
    x = x ^ arr[i];
    i = (i + 1) % 4;
  }
}

void rfc6455::assemble_frame(uint32_t mask_key, span<const char> data,
                             binary_buffer& out) {
  assemble_frame(text_frame, mask_key, as_bytes(data), out);
}

void rfc6455::assemble_frame(uint32_t mask_key, span<const byte> data,
                             binary_buffer& out) {
  assemble_frame(binary_frame, mask_key, data, out);
}

void rfc6455::assemble_frame(uint8_t opcode, uint32_t mask_key,
                             span<const byte> data, binary_buffer& out) {
  // First 8 bits: FIN flag + opcode (we never fragment frames).
  out.push_back(byte{static_cast<uint8_t>(0x80 | opcode)});
  // Mask flag + payload length (7 bits, 7+16 bits, or 7+64 bits)
  auto mask_bit = byte{static_cast<uint8_t>(mask_key == 0 ? 0x00 : 0x80)};
  if (data.size() < 126) {
    auto len = static_cast<uint8_t>(data.size());
    out.push_back(mask_bit | byte{len});
  } else if (data.size() < std::numeric_limits<uint16_t>::max()) {
    auto len = static_cast<uint16_t>(data.size());
    auto no_len = to_network_order(len);
    byte len_data[2];
    memcpy(len_data, &no_len, 2);
    out.push_back(mask_bit | byte{126});
    for (auto x : len_data)
      out.push_back(x);
  } else {
    auto len = static_cast<uint64_t>(data.size());
    auto no_len = to_network_order(len);
    byte len_data[8];
    memcpy(len_data, &no_len, 8);
    out.push_back(mask_bit | byte{127});
    out.insert(out.end(), len_data, len_data + 8);
  }
  // Masking key: 0 or 4 bytes.
  if (mask_key != 0) {
    auto no_key = to_network_order(mask_key);
    byte key_data[4];
    memcpy(key_data, &no_key, 4);
    out.insert(out.end(), key_data, key_data + 4);
  }
  // Application data.
  out.insert(out.end(), data.begin(), data.end());
}

ptrdiff_t rfc6455::decode_header(span<const byte> data, header& hdr) {
  if (data.size() < 2)
    return 0;
  auto byte1 = to_integer<uint8_t>(data[0]);
  auto byte2 = to_integer<uint8_t>(data[1]);
  // Fetch FIN flag and opcode.
  hdr.fin = (byte1 & 0x80) != 0;
  hdr.opcode = byte1 & 0x0F;
  // Decode mask bit and payload length field.
  bool masked = (byte2 & 0x80) != 0;
  auto len_field = byte2 & 0x7F;
  size_t header_length;
  if (len_field < 126) {
    header_length = 2 + (masked ? 4 : 0);
    hdr.payload_len = len_field;
  } else if (len_field == 126) {
    header_length = 4 + (masked ? 4 : 0);
  } else {
    header_length = 10 + (masked ? 4 : 0);
  }
  // Make sure we can read all the data we need.
  if (data.size() < header_length)
    return 0;
  // Start decoding remaining header bytes.
  const byte* p = data.data() + 2;
  // Fetch payload size
  if (len_field == 126) {
    uint16_t no_len;
    memcpy(&no_len, p, 2);
    hdr.payload_len = from_network_order(no_len);
    p += 2;
  } else if (len_field == 127) {
    uint64_t no_len;
    memcpy(&no_len, p, 8);
    hdr.payload_len = from_network_order(no_len);
    p += 8;
  }
  // Fetch mask key.
  if (masked) {
    uint32_t no_key;
    memcpy(&no_key, p, 4);
    hdr.mask_key = from_network_order(no_key);
    p += 4;
  } else {
    hdr.mask_key = 0;
  }
  // No extension bits allowed.
  if (byte1 & 0x70)
    return -1;
  // Verify opcode and return number of consumed bytes.
  switch (hdr.opcode) {
    case continuation_frame:
    case text_frame:
    case binary_frame:
    case connection_close:
    case ping:
    case pong:
      return static_cast<ptrdiff_t>(header_length);
    default:
      return -1;
  }
}

} // namespace caf::detail
