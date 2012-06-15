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


#ifndef CPPA_TYPE_TO_PTYPE_HPP
#define CPPA_TYPE_TO_PTYPE_HPP

#include <string>
#include <cstdint>
#include <type_traits>

#include "cppa/primitive_type.hpp"

namespace cppa { namespace detail {

// if (IfStmt == true) ptype = PT; else ptype = Else::ptype;
template<bool IfStmt, primitive_type PT, class Else>
struct if_else_ptype_c {
    static const primitive_type ptype = PT;
};

template<primitive_type PT, class Else>
struct if_else_ptype_c<false, PT, Else> {
    static const primitive_type ptype = Else::ptype;
};

// if (Stmt::value == true) ptype = FT; else ptype = Else::ptype;
template<class Stmt, primitive_type PT, class Else>
struct if_else_ptype : if_else_ptype_c<Stmt::value, PT, Else> { };

template<primitive_type PT>
struct wrapped_ptype { static const primitive_type ptype = PT; };

// maps type T the the corresponding fundamental_type
template<typename T>
struct type_to_ptype_impl :
    // signed integers
    if_else_ptype<std::is_same<T, std::int8_t>, pt_int8,
    if_else_ptype<std::is_same<T, std::int16_t>, pt_int16,
    if_else_ptype<std::is_same<T, std::int32_t>, pt_int32,
    if_else_ptype<std::is_same<T, std::int64_t>, pt_int64,
    // unsigned integers
    if_else_ptype<std::is_same<T, std::uint8_t>, pt_uint8,
    if_else_ptype<std::is_same<T, std::uint16_t>, pt_uint16,
    if_else_ptype<std::is_same<T, std::uint32_t>, pt_uint32,
    if_else_ptype<std::is_same<T, std::uint64_t>, pt_uint64,
    // floating points
    if_else_ptype<std::is_same<T, float>, pt_float,
    if_else_ptype<std::is_same<T, double>, pt_double,
    if_else_ptype<std::is_same<T, long double>, pt_long_double,
    // strings
    if_else_ptype<std::is_convertible<T, std::string>, pt_u8string,
    if_else_ptype<std::is_convertible<T, std::u16string>, pt_u16string,
    if_else_ptype<std::is_convertible<T, std::u32string>, pt_u32string,
    // default case
    wrapped_ptype<pt_null> > > > > > > > > > > > > > > {
};

template<typename T>
struct type_to_ptype {

    typedef typename std::remove_cv<typename std::remove_reference<T>::type>::type type;

    static const primitive_type ptype = type_to_ptype_impl<type>::ptype;

};

} } // namespace cppa::detail

#endif // CPPA_TYPE_TO_PTYPE_HPP
