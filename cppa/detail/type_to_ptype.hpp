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


#ifndef CPPA_TYPE_TO_PTYPE_HPP
#define CPPA_TYPE_TO_PTYPE_HPP

#include <string>
#include <limits>
#include <cstdint>
#include <type_traits>

#include "cppa/primitive_type.hpp"

#include "cppa/util/type_traits.hpp"

namespace cppa { namespace detail {

template<primitive_type PT>
struct wrapped_ptype { static const primitive_type ptype = PT; };

template<bool IsInteger, bool IsSigned, size_t Sizeof>
struct type_to_ptype_int {
    static constexpr primitive_type ptype = pt_null;
};

template<> struct type_to_ptype_int<true, true , 1> : wrapped_ptype<pt_int8  > { };
template<> struct type_to_ptype_int<true, false, 1> : wrapped_ptype<pt_uint8 > { };
template<> struct type_to_ptype_int<true, true , 2> : wrapped_ptype<pt_int16 > { };
template<> struct type_to_ptype_int<true, false, 2> : wrapped_ptype<pt_uint16> { };
template<> struct type_to_ptype_int<true, true , 4> : wrapped_ptype<pt_int32 > { };
template<> struct type_to_ptype_int<true, false, 4> : wrapped_ptype<pt_uint32> { };
template<> struct type_to_ptype_int<true, true , 8> : wrapped_ptype<pt_int64 > { };
template<> struct type_to_ptype_int<true, false, 8> : wrapped_ptype<pt_uint64> { };

template<bool IsArithmetic, typename T>
struct type_to_ptype_impl_helper : wrapped_ptype<pt_null> { };

template<typename T>
struct type_to_ptype_impl_helper<true, T> {
    static constexpr auto ptype = type_to_ptype_int<
                    std::numeric_limits<T>::is_integer,
                    std::numeric_limits<T>::is_signed,
                    sizeof(T)
                >::ptype;
};

// maps type T the the corresponding fundamental_type
template<typename T>
struct type_to_ptype_impl {
    static constexpr primitive_type ptype =
        std::is_convertible<T, std::string>::value
        ? pt_u8string
        : (std::is_convertible<T, std::u16string>::value
           ? pt_u16string
           : (std::is_convertible<T, std::u32string>::value
              ? pt_u32string
              : type_to_ptype_impl_helper<std::is_arithmetic<T>::value, T>::ptype));
};

// floating points
template<> struct type_to_ptype_impl<float>       : wrapped_ptype<pt_float      > { };
template<> struct type_to_ptype_impl<double>      : wrapped_ptype<pt_double     > { };
template<> struct type_to_ptype_impl<long double> : wrapped_ptype<pt_long_double> { };
template<> struct type_to_ptype_impl<atom_value>  : wrapped_ptype<pt_atom       > { };

template<typename T>
struct type_to_ptype : type_to_ptype_impl<typename util::rm_const_and_ref<T>::type> { };

} } // namespace cppa::detail

#endif // CPPA_TYPE_TO_PTYPE_HPP
