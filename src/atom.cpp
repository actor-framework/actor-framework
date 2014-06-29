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

#include "cppa/atom.hpp"

namespace cppa {

std::string to_string(const atom_value& what) {
    auto x = static_cast<uint64_t>(what);
    std::string result;
    result.reserve(11);
    // don't read characters before we found the leading 0xF
    // first four bits set?
    bool read_chars = ((x & 0xF000000000000000) >> 60) == 0xF;
    uint64_t mask = 0x0FC0000000000000;
    for (int bitshift = 54; bitshift >= 0; bitshift -= 6, mask >>= 6) {
        if (read_chars) {
            result += detail::decoding_table[(x & mask) >> bitshift];
        } else if (((x & mask) >> bitshift) == 0xF) {
            read_chars = true;
        }
    }
    return result;
}

} // namespace cppa
