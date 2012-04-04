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


#ifndef STATIC_FOREACH_HPP
#define STATIC_FOREACH_HPP

#include "cppa/get.hpp"

namespace cppa { namespace util {

template<size_t Begin, size_t End, bool BeginGreaterEnd>
struct static_foreach_impl
{
    template<typename Container, typename Fun>
    static inline void _(Container const& c, Fun& f)
    {
        f(get<Begin>(c));
        static_foreach_impl<Begin+1, End, (Begin+1 > End)>::_(c, f);
    }
    template<typename Container, typename Fun>
    static inline bool eval(Container const& c, Fun& f)
    {
        return    f(get<Begin>(c))
               && static_foreach_impl<Begin+1, End, (Begin+1 > End)>::eval(c, f);
    }
};

template<size_t X>
struct static_foreach_impl<X, X, false>
{
    template<typename Container, typename Fun>
    static inline void _(Container const&, Fun&) { }
    template<typename Container, typename Fun>
    static inline bool eval(Container const&, Fun&) { return true; }
};

template<size_t X, size_t Y>
struct static_foreach_impl<X, Y, true>
{
    template<typename Container, typename Fun>
    static inline void _(Container const&, Fun&) { }
    template<typename Container, typename Fun>
    static inline bool eval(Container const&, Fun&) { return true; }
};

/**
 * @ingroup MetaProgramming
 * @brief A for loop that can be used with tuples.
 */
template<size_t Begin, size_t End>
struct static_foreach : static_foreach_impl<Begin, End, (Begin > End)>
{
};

} } // namespace cppa::util

#endif // STATIC_FOREACH_HPP
