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


#ifndef CPPA_TO_STRING_HPP
#define CPPA_TO_STRING_HPP

#include <type_traits>

#include "cppa/atom.hpp" // included for to_string(atom_value)
#include "cppa/actor.hpp"
#include "cppa/group.hpp"
#include "cppa/object.hpp"
#include "cppa/channel.hpp"
#include "cppa/anything.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/intrusive_ptr.hpp"
#include "cppa/uniform_type_info.hpp"
#include "cppa/process_information.hpp"

namespace cppa {

namespace detail {

std::string to_string_impl(const void* what, const uniform_type_info* utype);

template<typename T>
struct has_cppa_to_string : std::false_type { };

template<>
struct has_cppa_to_string<any_tuple> : std::true_type { };

template<>
struct has_cppa_to_string<addressed_message> : std::true_type { };

template<>
struct has_cppa_to_string<actor_ptr> : std::true_type { };

template<>
struct has_cppa_to_string<group_ptr> : std::true_type { };

template<>
struct has_cppa_to_string<channel_ptr> : std::true_type { };

template<>
struct has_cppa_to_string<process_information_ptr> : std::true_type { };

} // namespace detail

/**
 * @brief Converts any of libcppa's core types to a string.
 */
template<typename T>
inline std::string to_string(const T& what,
                             typename std::enable_if<detail::has_cppa_to_string<T>::value>::type* = 0) {
    return detail::to_string_impl(&what, uniform_typeid<T>());
}

/**
 * @brief Converts an object to a string.
 */
inline std::string to_string(const object& what) {
    return detail::to_string_impl(what.value(), what.type());
}

} // namespace cppa

#endif // CPPA_TO_STRING_HPP
