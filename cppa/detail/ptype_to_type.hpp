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
 * Copyright (C) 2011-2013                                                    *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation; either version 2.1 of the License,               *
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


#ifndef CPPA_DETAIL_PTYPE_TO_TYPE_HPP
#define CPPA_DETAIL_PTYPE_TO_TYPE_HPP

#include <cstdint>

#include "cppa/primitive_type.hpp"

#include "cppa/util/wrapped.hpp"

namespace cppa {
namespace detail {

// maps the primitive_type PT to the corresponding type
template<primitive_type PT>
struct ptype_to_type : util::wrapped<void> { };

// libcppa types
template<> struct ptype_to_type<pt_atom  > : util::wrapped<atom_value   > { };

// integer types
template<> struct ptype_to_type<pt_int8  > : util::wrapped<std::int8_t  > { };
template<> struct ptype_to_type<pt_uint8 > : util::wrapped<std::uint8_t > { };
template<> struct ptype_to_type<pt_int16 > : util::wrapped<std::int16_t > { };
template<> struct ptype_to_type<pt_uint16> : util::wrapped<std::uint16_t> { };
template<> struct ptype_to_type<pt_int32 > : util::wrapped<std::int32_t > { };
template<> struct ptype_to_type<pt_uint32> : util::wrapped<std::uint32_t> { };
template<> struct ptype_to_type<pt_int64 > : util::wrapped<std::int64_t > { };
template<> struct ptype_to_type<pt_uint64> : util::wrapped<std::uint64_t> { };

// floating points
template<> struct ptype_to_type<pt_float>       : util::wrapped<float> { };
template<> struct ptype_to_type<pt_double>      : util::wrapped<double> { };
template<> struct ptype_to_type<pt_long_double> : util::wrapped<long double> { };

// strings
template<> struct ptype_to_type<pt_u8string > : util::wrapped<std::string   > { };
template<> struct ptype_to_type<pt_u16string> : util::wrapped<std::u16string> { };
template<> struct ptype_to_type<pt_u32string> : util::wrapped<std::u32string> { };

} // namespace detail
} // namespace cppa

#endif // CPPA_DETAIL_PTYPE_TO_TYPE_HPP
