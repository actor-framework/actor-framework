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


#ifndef CPPA_PTYPE_TO_TYPE_HPP
#define CPPA_PTYPE_TO_TYPE_HPP

#include <cstdint>

#include "cppa/primitive_type.hpp"

#include "cppa/util/if_else.hpp"
#include "cppa/util/wrapped.hpp"

namespace cppa { namespace detail {

// maps the primitive_type PT to the corresponding type
template<primitive_type PT>
struct ptype_to_type :
    // signed integers
    util::if_else_c<PT == pt_int8, std::int8_t,
    util::if_else_c<PT == pt_int16, std::int16_t,
    util::if_else_c<PT == pt_int32, std::int32_t,
    util::if_else_c<PT == pt_int64, std::int64_t,
    util::if_else_c<PT == pt_uint8, std::uint8_t,
    // unsigned integers
    util::if_else_c<PT == pt_uint16, std::uint16_t,
    util::if_else_c<PT == pt_uint32, std::uint32_t,
    util::if_else_c<PT == pt_uint64, std::uint64_t,
    // floating points
    util::if_else_c<PT == pt_float, float,
    util::if_else_c<PT == pt_double, double,
    util::if_else_c<PT == pt_long_double, long double,
    // strings
    util::if_else_c<PT == pt_u8string, std::string,
    util::if_else_c<PT == pt_u16string, std::u16string,
    util::if_else_c<PT == pt_u32string, std::u32string,
    // default case
    void > > > > > > > > > > > > > > {
};

} } // namespace cppa::detail

#endif // CPPA_PTYPE_TO_TYPE_HPP
