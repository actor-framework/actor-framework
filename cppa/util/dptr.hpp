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


#ifndef CPPA_DTHIS_HPP
#define CPPA_DTHIS_HPP

#include <type_traits>

namespace cppa {
namespace util {

/**
 * @brief Returns <tt>static_cast<Subtype*>(ptr)</tt> if @p Subtype is a
 *        derived type of @p MixinType, returns @p ptr without cast otherwise.
 */
template<class Subtype, class MixinType>
typename std::conditional<
    std::is_base_of<MixinType, Subtype>::value,
    Subtype,
    MixinType
>::type*
dptr(MixinType* ptr) {
    typedef typename std::conditional<
            std::is_base_of<MixinType, Subtype>::value,
            Subtype,
            MixinType
        >::type
        result_type;
    return static_cast<result_type*>(ptr);
}

} // namespace util
} // namespace cppa


#endif // CPPA_DTHIS_HPP
