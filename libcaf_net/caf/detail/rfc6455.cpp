// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/rfc6455.hpp"

#include "caf/detail/network_order.hpp"

#include <cstring>

namespace caf::detail {

void rfc6455::mask_data(uint32_t key, span<char> data, size_t offset) {
  mask_data(key, as_writable_bytes(data), offset);
}

void rfc6455::mask_data(uint32_t key, byte_span data, size_t offset) {
  auto no_key = to_network_order(key);
  std::byte arr[4];
  memcpy(arr, &no_key, 4);
  data = data.subspan(offset);
  auto i = offset % 4;
  for (auto& x : data) {
    x = x ^ arr[i];
    i = (i + 1) % 4;
  }
}

void rfc6455::assemble_frame(uint32_t mask_key, span<const char> data,
                             byte_buffer& out) {
  assemble_frame(text_frame, mask_key, as_bytes(data), out);
}

void rfc6455::assemble_frame(uint32_t mask_key, const_byte_span data,
                             byte_buffer& out) {
  assemble_frame(binary_frame, mask_key, data, out);
}

void rfc6455::assemble_frame(uint8_t opcode, uint32_t mask_key,
                             const_byte_span data, byte_buffer& out,
                             uint8_t flags) {
  // First 8 bits: flags + opcode
  out.push_back(std::byte{static_cast<uint8_t>(flags | opcode)});
  // Mask flag + payload length (7 bits, 7+16 bits, or 7+64 bits)
  auto mask_bit = std::byte{static_cast<uint8_t>(mask_key == 0 ? 0x00 : 0x80)};
  if (data.size() < 126) {
    auto len = static_cast<uint8_t>(data.size());
    out.push_back(mask_bit | std::byte{len});
  } else if (data.size() <= std::numeric_limits<uint16_t>::max()) {
    auto len = static_cast<uint16_t>(data.size());
    auto no_len = to_network_order(len);
    std::byte len_data[2];
    memcpy(len_data, &no_len, 2);
    out.push_back(mask_bit | std::byte{126});
    for (auto x : len_data)
      out.push_back(x);
  } else {
    auto len = static_cast<uint64_t>(data.size());
    auto no_len = to_network_order(len);
    std::byte len_data[8];
    memcpy(len_data, &no_len, 8);
    out.push_back(mask_bit | std::byte{127});
    out.insert(out.end(), len_data, len_data + 8);
  }
  // Masking key: 0 or 4 bytes.
  if (mask_key != 0) {
    auto no_key = to_network_order(mask_key);
    std::byte key_data[4];
    memcpy(key_data, &no_key, 4);
    out.insert(out.end(), key_data, key_data + 4);
  }
  // Application data.
  out.insert(out.end(), data.begin(), data.end());
}

ptrdiff_t rfc6455::decode_header(const_byte_span data, header& hdr) {
  if (data.size() < 2)
    return 0;
  auto byte1 = std::to_integer<uint8_t>(data[0]);
  auto byte2 = std::to_integer<uint8_t>(data[1]);
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
  const std::byte* p = data.data() + 2;
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
