/******************************************************************************\
 *           ___        __                                                    *
 *          /\_ \    __/\ \                                                   *
 *          \//\ \  /\_\ \ \____    ___   _____   _____      __               *
 *            \ \ \ \/\ \ \ '__`\  /'___\/\ '__`\/\ '__`\  /'__`\             *
 *             \_\ \_\ \ \ \ \L\ \/\ \__/\ \ \L\ \ \ \L\ \/\ \L\.\_           *
 *             /\____\\ \_\ \_,__/\ \____\\ \ ,__/\ \ ,__/\ \__/.\_\          *
 *             \/____/ \/_/\/___/  \/____/ \ \ \/  \ \ \/  \/__/\/_/          *
 *                                          \ \_\   \ \_\                     *
 *                                           \/_/    \/_/                     *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/

#ifndef CAF_DETAIL_ATOM_VAL_HPP
#define CAF_DETAIL_ATOM_VAL_HPP

namespace caf {
namespace detail {

namespace {

// encodes ASCII characters to 6bit encoding
constexpr unsigned char encoding_table[] = {
/*         ..0 ..1 ..2 ..3 ..4 ..5 ..6 ..7 ..8 ..9 ..A ..B ..C ..D ..E ..F    */
/* 0.. */  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
/* 1.. */  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
/* 2.. */  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
/* 3.. */  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 0,  0,  0,  0,  0,  0,
/* 4.. */  0, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
/* 5.. */ 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36,  0,  0,  0,  0, 37,
/* 6.. */  0, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52,
/* 7.. */ 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,  0,  0,  0,  0,  0};

// decodes 6bit characters to ASCII
constexpr char decoding_table[] = " 0123456789"
                                  "ABCDEFGHIJKLMNOPQRSTUVWXYZ_"
                                  "abcdefghijklmnopqrstuvwxyz";

} // namespace <anonymous>

constexpr uint64_t next_interim(uint64_t current, size_t char_code) {
    return (current << 6) | encoding_table[(char_code <= 0x7F) ? char_code : 0];
}

constexpr uint64_t atom_val(const char* cstr, uint64_t interim = 0) {
    return (*cstr == '\0') ?
               interim :
               atom_val(cstr + 1,
                        next_interim(interim, static_cast<size_t>(*cstr)));
}

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_ATOM_VAL_HPP
