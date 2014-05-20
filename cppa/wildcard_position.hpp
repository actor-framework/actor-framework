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
 * Copyright (C) 2011-2014                                                    *
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


#ifndef CPPA_WILDCARD_POSITION_HPP
#define CPPA_WILDCARD_POSITION_HPP

#include <type_traits>

#include "cppa/anything.hpp"
#include "cppa/util/type_list.hpp"

namespace cppa {

/**
 * @brief Denotes the position of {@link cppa::anything anything} in a
 *        template parameter pack.
 */
enum class wildcard_position {
    nil,
    trailing,
    leading,
    in_between,
    multiple
};

/**
 * @brief Gets the position of {@link cppa::anything anything} from the
 *        type list @p Types.
 * @tparam A template parameter pack as {@link cppa::util::type_list type_list}.
 */
template<typename Types>
constexpr wildcard_position get_wildcard_position() {
    return util::tl_count<Types, is_anything>::value > 1
           ? wildcard_position::multiple
           : (util::tl_count<Types, is_anything>::value == 1
              ? (std::is_same<typename util::tl_head<Types>::type, anything>::value
                 ? wildcard_position::leading
                 : (std::is_same<typename util::tl_back<Types>::type, anything>::value
                    ? wildcard_position::trailing
                    : wildcard_position::in_between))
              : wildcard_position::nil);
}

} // namespace cppa

#endif // CPPA_WILDCARD_POSITION_HPP
