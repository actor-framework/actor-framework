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
 * Copyright (C) 2011, 2012                                                   *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation, either version 3 of the License                  *
 * or (at your option) any later version.                                     *
 *                                                                            *
 * libcppa is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                       *
 * See the GNU Lesser General Public License for more details.                *
 *                                                                            *
 * You should have received a copy of the GNU Lesser General Public License   *
 * along with libcppa. If not, see <http://www.gnu.org/licenses/>.            *
\******************************************************************************/


#ifndef ATOM_VAL_HPP
#define ATOM_VAL_HPP

namespace cppa { namespace detail { namespace {

// encodes ASCII characters to 6bit encoding
constexpr char encoding_table[] =
{
/*         ..0 ..1 ..2 ..3 ..4 ..5 ..6 ..7 ..8 ..9 ..A ..B ..C ..D ..E ..F    */
/* 0.. */    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
/* 1.. */    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
/* 2.. */    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
/* 3.. */    1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11,  0,  0,  0,  0,  0,
/* 4.. */    0, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26,
/* 5.. */   27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37,  0,  0,  0,  0, 38,
/* 6.. */    0, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53,
/* 7.. */   54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64,  0,  0,  0,  0,  0
};

// decodes 6bit characters to ASCII
constexpr char decoding_table[] = " 0123456789:"
                                  "ABCDEFGHIJKLMNOPQRSTUVWXYZ_"
                                  "abcdefghijklmnopqrstuvwxyz";

inline constexpr std::uint64_t next_interim(std::uint64_t tmp, size_t char_code)
{
    return (tmp << 6) | encoding_table[char_code];
}

constexpr std::uint64_t atom_val(char const* str, std::uint64_t interim = 0)
{
    return (*str <= 0) ? interim
                       : atom_val(str + 1, next_interim(interim, *str));
}

} } } // namespace cppa::detail::<anonymous>

#endif // ATOM_VAL_HPP
