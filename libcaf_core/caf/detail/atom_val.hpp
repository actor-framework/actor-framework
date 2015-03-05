/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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

#ifndef CAF_DETAIL_ATOM_VAL_HPP
#define CAF_DETAIL_ATOM_VAL_HPP

#include <cstdint>
#include <type_traits>

#include "caf/config.hpp"

namespace caf {
namespace detail {

// encoding table from ASCII to 6bit encoding for atoms
//     ..0 ..1 ..2 ..3 ..4 ..5 ..6 ..7 ..8 ..9 ..A ..B ..C ..D ..E ..F
// 0..   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
// 1..   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
// 2..   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
// 3..   1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 0,  0,  0,  0,  0,  0,
// 4..   0, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
// 5..  26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36,  0,  0,  0,  0, 37,
// 6..   0, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52,
// 7..  53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,  0,  0,  0,  0,  0

constexpr char atom_decode(uint64_t value) {
  return value > 0 && value < 11 // 1..10 => '0'..'9'
         ? static_cast<char>('0' + value - 1)
         : value > 10 && value < 37 // 11..36 => 'A'..'Z'
           ? static_cast<char>('A' + value - 11)
           : value == 37
             ? '_'
             : value > 37 && value < 64
               ? static_cast<char>('a' + value - 38)
               : ' '; // everything else is mapped to whitespace
}

constexpr uint64_t atom_encode(char c) {
  return c >= '0' && c <= '9' // '0'..'9' => 1..10
         ? static_cast<uint64_t>(c - '0' + 1)
         : c >= 'A' && c <= 'Z' // 'A'..'Z' => 11..36
           ? static_cast<uint64_t>(c - 'A' + 11)
           : c == '_' // _ => 37
             ? 37
             : c >= 'a' && c <= 'z' // 'a'..'z' => 38..63
               ? static_cast<uint64_t>(c - 'a' + 38)
               : 0; // everything else is marked invalid
}

#ifdef CAF_MSVC
constexpr uint64_t atom_val(size_t pos, size_t cstr_size,
                            const char* cstr, uint64_t interim = 0xF) {
  return pos == cstr_size
         ? interim
         : atom_val(pos + 1, cstr_size, cstr,
                    (interim << 6) | atom_encode(*cstr));
}
#else
constexpr uint64_t atom_val(const char* cstr, uint64_t val = 0xF) {
  return *cstr == '\0'
         ? val
         : atom_val(cstr + 1, (val << 6) | atom_encode(*cstr));
}
#endif

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_ATOM_VAL_HPP
