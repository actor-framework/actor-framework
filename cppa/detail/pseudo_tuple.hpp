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
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/


#ifndef CPPA_DETAIL_PSEUDO_TUPLE_HPP
#define CPPA_DETAIL_PSEUDO_TUPLE_HPP

#include <cstddef>

#include "cppa/util/type_traits.hpp"

namespace cppa {
namespace detail {

// tuple-like access to an array of void pointers
template<typename... T>
struct pseudo_tuple {
    typedef void* pointer;
    typedef const void* const_pointer;

    pointer data[sizeof...(T) > 0 ? sizeof...(T) : 1];

    inline const_pointer at(size_t p) const {
        return data[p];
    }

    inline pointer mutable_at(size_t p) {
        return data[p];
    }

    inline pointer& operator[](size_t p) {
        return data[p];
    }
};

template<class List>
struct pseudo_tuple_from_type_list;

template<typename... Ts>
struct pseudo_tuple_from_type_list<util::type_list<Ts...> > {
    typedef pseudo_tuple<Ts...> type;
};

} // namespace detail
} // namespace cppa

namespace cppa {

template<size_t N, typename... Ts>
const typename util::type_at<N, Ts...>::type& get(const detail::pseudo_tuple<Ts...>& tv) {
    static_assert(N < sizeof...(Ts), "N >= tv.size()");
    return *reinterpret_cast<const typename util::type_at<N, Ts...>::type*>(tv.at(N));
}

template<size_t N, typename... Ts>
typename util::type_at<N, Ts...>::type& get_ref(detail::pseudo_tuple<Ts...>& tv) {
    static_assert(N < sizeof...(Ts), "N >= tv.size()");
    return *reinterpret_cast<typename util::type_at<N, Ts...>::type*>(tv.mutable_at(N));
}

} // namespace cppa

#endif // CPPA_DETAIL_PSEUDO_TUPLE_HPP
