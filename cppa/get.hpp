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


#ifndef CPPA_GET_HPP
#define CPPA_GET_HPP

// functions are documented in the implementation headers
#ifndef CPPA_DOCUMENTATION

#include <cstddef>

#include "cppa/util/at.hpp"
#include "cppa/util/type_list.hpp"

namespace cppa {

// forward declaration of details
namespace detail {
template<typename...> struct tdata;
template<typename...> struct pseudo_tuple;
}

// forward declaration of cow_tuple
template<typename...> class cow_tuple;

// forward declaration of get(const detail::tdata<...>&)
template<size_t N, typename... Tn>
const typename util::at<N, Tn...>::type& get(const detail::tdata<Tn...>&);

// forward declarations of get(const tuple<...>&)
template<size_t N, typename... Tn>
const typename util::at<N, Tn...>::type& get(const cow_tuple<Tn...>&);

// forward declarations of get(detail::pseudo_tuple<...>&)
template<size_t N, typename... Tn>
const typename util::at<N, Tn...>::type& get(const detail::pseudo_tuple<Tn...>& tv);

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
typename util::at<N, Ts...>::type get(const util::type_list<Ts...>&) {
    return {};
}

} // namespace cppa

#endif // CPPA_DOCUMENTATION
#endif // CPPA_GET_HPP
