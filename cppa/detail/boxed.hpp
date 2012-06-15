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


#ifndef CPPA_BOXED_HPP
#define CPPA_BOXED_HPP

#include "cppa/anything.hpp"
#include "cppa/util/wrapped.hpp"

namespace cppa { namespace detail {

template<typename T>
struct boxed {
    typedef util::wrapped<T> type;
};

template<typename T>
struct boxed< util::wrapped<T> > {
    typedef util::wrapped<T> type;
};

template<>
struct boxed<anything> {
    typedef anything type;
};

template<typename T>
struct is_boxed {
    static constexpr bool value = false;
};

template<typename T>
struct is_boxed< util::wrapped<T> > {
    static constexpr bool value = true;
};

template<typename T>
struct is_boxed<util::wrapped<T>()> {
    static constexpr bool value = true;
};

template<typename T>
struct is_boxed<util::wrapped<T>(&)()> {
    static constexpr bool value = true;
};

template<typename T>
struct is_boxed<util::wrapped<T>(*)()> {
    static constexpr bool value = true;
};

} } // namespace cppa::detail

#endif // CPPA_BOXED_HPP
