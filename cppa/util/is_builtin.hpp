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


#ifndef CPPA_IS_BUILTIN_HPP
#define CPPA_IS_BUILTIN_HPP

#include <string>
#include <type_traits>

#include "cppa/atom.hpp"
#include "cppa/actor.hpp"
#include "cppa/group.hpp"
#include "cppa/channel.hpp"
#include "cppa/anything.hpp"
#include "cppa/intrusive_ptr.hpp"
#include "cppa/process_information.hpp"

namespace cppa { class any_tuple; }
namespace cppa { namespace detail { class addressed_message; } }

namespace cppa { namespace util {

template<typename T>
struct is_builtin {
    static constexpr bool value = std::is_arithmetic<T>::value;
};

template<>
struct is_builtin<anything> : std::true_type { };

template<>
struct is_builtin<std::string> : std::true_type { };

template<>
struct is_builtin<std::u16string> : std::true_type { };

template<>
struct is_builtin<std::u32string> : std::true_type { };

template<>
struct is_builtin<atom_value> : std::true_type { };

template<>
struct is_builtin<any_tuple> : std::true_type { };

template<>
struct is_builtin<detail::addressed_message> : std::true_type { };

template<>
struct is_builtin<actor_ptr> : std::true_type { };

template<>
struct is_builtin<group_ptr> : std::true_type { };

template<>
struct is_builtin<channel_ptr> : std::true_type { };

template<>
struct is_builtin<intrusive_ptr<process_information> > : std::true_type { };

} } // namespace cppa::util

#endif // CPPA_IS_BUILTIN_HPP
