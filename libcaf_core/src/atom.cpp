/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/atom.hpp"

#include <array>
#include <cstring>

#include "caf/string_view.hpp"

namespace caf {

namespace {

/// A buffer for decoding atom values.
using atom_value_buf = std::array<char, 11>;

size_t decode(atom_value_buf& buf, atom_value what) {
  size_t pos = 0;
  auto x = static_cast<uint64_t>(what);
  // Don't read characters before we found the leading 0xF.
  bool read_chars = ((x & 0xF000000000000000) >> 60) == 0xF;
  uint64_t mask = 0x0FC0000000000000;
  for (int bitshift = 54; bitshift >= 0; bitshift -= 6, mask >>= 6) {
    if (read_chars)
      buf[pos++] = detail::decoding_table[(x & mask) >> bitshift];
    else if (((x & mask) >> bitshift) == 0xF)
      read_chars = true;
  }
  buf[pos] = '\0';
  return pos;
}

} // namespace <anonymous>

atom_value to_lowercase(atom_value x) {
  atom_value_buf buf;
  decode(buf, x);
  for (auto ch = buf.data(); *ch != '\0'; ++ch)
    *ch = static_cast<char>(tolower(*ch));
  return static_cast<atom_value>(detail::atom_val(buf.data()));
}

atom_value atom_from_string(string_view x) {
  if (x.size() > 10)
    return atom("");
  atom_value_buf buf;
  memcpy(buf.data(), x.data(), x.size());
  buf[x.size()] = '\0';
  return static_cast<atom_value>(detail::atom_val(buf.data()));
}

std::string to_string(const atom_value& x) {
  atom_value_buf str;
  auto len = decode(str, x);
  return std::string(str.begin(), str.begin() + len);
}

} // namespace caf
