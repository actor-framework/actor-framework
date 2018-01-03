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

#include <cstring>

namespace caf {

atom_value atom_from_string(const std::string& x) {
  if (x.size() > 10)
    return atom("");
  char buf[11];
  memcpy(buf, x.c_str(), x.size());
  buf[x.size()] = '\0';
  return atom(buf);
}

std::string to_string(const atom_value& what) {
  auto x = static_cast<uint64_t>(what);
  std::string result;
  result.reserve(11);
  // don't read characters before we found the leading 0xF
  // first four bits set?
  bool read_chars = ((x & 0xF000000000000000) >> 60) == 0xF;
  uint64_t mask = 0x0FC0000000000000;
  for (int bitshift = 54; bitshift >= 0; bitshift -= 6, mask >>= 6) {
    if (read_chars)
      result += detail::decoding_table[(x & mask) >> bitshift];
    else if (((x & mask) >> bitshift) == 0xF)
      read_chars = true;
  }
  return result;
}

} // namespace caf
