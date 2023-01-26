// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <array>
#include <cstdint>

namespace caf::detail {

/// Sets all bits after bits_to_keep to 0.
template <size_t NumBytes>
void mask_bits(std::array<uint8_t, NumBytes>& bytes, size_t bits_to_keep) {
  // Calculate how many bytes we keep.
  auto bytes_to_keep = bits_to_keep / 8;
  if (bytes_to_keep >= NumBytes)
    return;
  // See whether we have an unclean cut, e.g. keeping 7 bits of a byte.
  auto byte_cutoff = bits_to_keep % 8;
  auto i = bytes.begin() + bytes_to_keep;
  if (byte_cutoff != 0) {
    static constexpr uint8_t mask[] = {0x00, 0x80, 0xC0, 0xE0,
                                       0xF0, 0xF8, 0xFC, 0xFE};
    *i = *i & mask[byte_cutoff];
    ++i;
  }
  // Zero remaining bytes.
  for (; i != bytes.end(); ++i)
    *i = 0;
}

} // namespace caf::detail
