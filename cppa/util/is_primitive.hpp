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


#ifndef CPPA_IS_PRIMITIVE_HPP
#define CPPA_IS_PRIMITIVE_HPP

#include "cppa/detail/type_to_ptype.hpp"

namespace cppa { namespace util {

/**
 * @ingroup MetaProgramming
 * @brief Chekcs wheter @p T is a primitive type.
 *
 * <code>is_primitive<T>::value == true</code> if and only if @c T
 * is a signed / unsigned integer or one of the following types:
 * - @c float
 * - @c double
 * - @c long @c double
 * - @c std::string
 * - @c std::u16string
 * - @c std::u32string
 */
template<typename T>
struct is_primitive {
    static constexpr bool value = detail::type_to_ptype<T>::ptype != pt_null;
};

} } // namespace cppa::util

#endif // CPPA_IS_PRIMITIVE_HPP
