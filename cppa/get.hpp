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


#ifndef GET_HPP
#define GET_HPP

#include <cstddef>

#include "cppa/util/at.hpp"
#include "cppa/util/type_list.hpp"

namespace cppa {

// forward declaration of details
namespace detail {
template<typename...> class tdata;
template<typename...> struct pseudo_tuple;
}

// forward declaration of cow_tuple
template<typename...> class cow_tuple;

// forward declaration of get(detail::tdata<...> const&)
template<size_t N, typename... Tn>
typename util::at<N, Tn...>::type const& get(detail::tdata<Tn...> const&);

// forward declarations of get(tuple<...> const&)
template<size_t N, typename... Tn>
typename util::at<N, Tn...>::type const& get(cow_tuple<Tn...> const&);

// forward declarations of get(detail::pseudo_tuple<...>&)
template<size_t N, typename... Tn>
typename util::at<N, Tn...>::type const& get(detail::pseudo_tuple<Tn...> const& tv);

// forward declarations of get_ref(detail::tdata<...>&)
template<size_t N, typename... Tn>
typename util::at<N, Tn...>::type& get_ref(detail::tdata<Tn...>&);

// forward declarations of get_ref(tuple<...>&)
template<size_t N, typename... Tn>
typename util::at<N, Tn...>::type& get_ref(cow_tuple<Tn...>&);

// forward declarations of get_ref(detail::pseudo_tuple<...>&)
template<size_t N, typename... Tn>
typename util::at<N, Tn...>::type& get_ref(detail::pseudo_tuple<Tn...>& tv);

// support container-like access for type lists containing tokens
template<size_t N, typename... Ts>
typename util::at<N, Ts...>::type get(util::type_list<Ts...> const&)
{
    return {};
}

} // namespace cppa

#endif // GET_HPP
