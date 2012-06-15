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


#ifndef CPPA_PSEUDO_TUPLE_HPP
#define CPPA_PSEUDO_TUPLE_HPP

#include "cppa/util/at.hpp"

namespace cppa { namespace detail {

template<typename... T>
struct pseudo_tuple {
    typedef void* ptr_type;
    typedef const void* const_ptr_type;

    ptr_type data[sizeof...(T) > 0 ? sizeof...(T) : 1];

    inline const_ptr_type at(size_t p) const {
        return data[p];
    }

    inline ptr_type mutable_at(size_t p) {
        return const_cast<ptr_type>(data[p]);
    }

    inline void*& operator[](size_t p) {
        return data[p];
    }
};

template<class List>
struct pseudo_tuple_from_type_list;

template<typename... Ts>
struct pseudo_tuple_from_type_list<util::type_list<Ts...> > {
    typedef pseudo_tuple<Ts...> type;
};

} } // namespace cppa::detail

namespace cppa {

template<size_t N, typename... Tn>
const typename util::at<N, Tn...>::type& get(const detail::pseudo_tuple<Tn...>& tv) {
    static_assert(N < sizeof...(Tn), "N >= tv.size()");
    return *reinterpret_cast<const typename util::at<N, Tn...>::type*>(tv.at(N));
}

template<size_t N, typename... Tn>
typename util::at<N, Tn...>::type& get_ref(detail::pseudo_tuple<Tn...>& tv) {
    static_assert(N < sizeof...(Tn), "N >= tv.size()");
    return *reinterpret_cast<typename util::at<N, Tn...>::type*>(tv.mutable_at(N));
}

} // namespace cppa

#endif // CPPA_PSEUDO_TUPLE_HPP
