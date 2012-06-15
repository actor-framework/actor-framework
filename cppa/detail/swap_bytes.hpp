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


#ifndef CPPA_SWAP_BYTES_HPP
#define CPPA_SWAP_BYTES_HPP

#include <cstddef>

namespace cppa { namespace detail {

template<typename T>
struct byte_access {
    union {
        T value;
        unsigned char bytes[sizeof(T)];
    };
    inline byte_access(T val = 0) : value(val) { }
};

template<size_t SizeOfT, typename T>
struct byte_swapper {
    static T _(byte_access<T> from) {
        byte_access<T> to;
        auto i = SizeOfT - 1;
        for (size_t j = 0 ; j < SizeOfT ; --i, ++j) {
            to.bytes[i] = from.bytes[j];
        }
        return to.value;
    }
};

template<typename T>
struct byte_swapper<1, T> {
    inline static T _(T what) { return what; }
};

template<typename T>
inline T swap_bytes(T what) {
    return byte_swapper<sizeof(T), T>::_(what);
}

} } // namespace cppa::detail

#endif // CPPA_SWAP_BYTES_HPP
