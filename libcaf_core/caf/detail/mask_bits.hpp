/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
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

#include <array>
#include <cstdint>

namespace caf {
namespace detail {

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

} // namespace detail
} // namespace caf


