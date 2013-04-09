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
#include <cstdint>
#include <type_traits>

#include "cppa/primitive_type.hpp"

#include "cppa/util/rm_ref.hpp"

namespace cppa { namespace detail {

template<primitive_type PT>
struct wrapped_ptype { static const primitive_type ptype = PT; };

// maps type T the the corresponding fundamental_type
template<typename T>
struct type_to_ptype_impl {
    static constexpr primitive_type ptype =
        std::is_convertible<T,std::string>::value
        ? pt_u8string
        : (std::is_convertible<T,std::u16string>::value
           ? pt_u16string
           : (std::is_convertible<T,std::u32string>::value
              ? pt_u32string
              : pt_null));
};

// integers
template<> struct type_to_ptype_impl<std::int8_t  > : wrapped_ptype<pt_int8  > { };
template<> struct type_to_ptype_impl<std::uint8_t > : wrapped_ptype<pt_uint8 > { };
template<> struct type_to_ptype_impl<std::int16_t > : wrapped_ptype<pt_int16 > { };
template<> struct type_to_ptype_impl<std::uint16_t> : wrapped_ptype<pt_uint16> { };
template<> struct type_to_ptype_impl<std::int32_t > : wrapped_ptype<pt_int32 > { };
template<> struct type_to_ptype_impl<std::uint32_t> : wrapped_ptype<pt_uint32> { };
template<> struct type_to_ptype_impl<std::int64_t > : wrapped_ptype<pt_int64 > { };
template<> struct type_to_ptype_impl<std::uint64_t> : wrapped_ptype<pt_uint64> { };

// floating points
template<> struct type_to_ptype_impl<float>       : wrapped_ptype<pt_float      > { };
template<> struct type_to_ptype_impl<double>      : wrapped_ptype<pt_double     > { };
template<> struct type_to_ptype_impl<long double> : wrapped_ptype<pt_long_double> { };

template<typename T>
struct type_to_ptype : type_to_ptype_impl<typename util::rm_ref<T>::type> { };

} } // namespace cppa::detail

#endif // CPPA_TYPE_TO_PTYPE_HPP
