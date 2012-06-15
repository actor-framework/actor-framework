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


#ifndef CPPA_UNBOXED_HPP
#define CPPA_UNBOXED_HPP

#include <memory>

#include "cppa/util/guard.hpp"
#include "cppa/util/wrapped.hpp"

namespace cppa { namespace detail {

template<typename T>
struct unboxed {
    typedef T type;
};

template<typename T>
struct unboxed< util::wrapped<T> > {
    typedef typename util::wrapped<T>::type type;
};

template<typename T>
struct unboxed<util::wrapped<T> (&)()> {
    typedef typename util::wrapped<T>::type type;
};

template<typename T>
struct unboxed<util::wrapped<T> ()> {
    typedef typename util::wrapped<T>::type type;
};

template<typename T>
struct unboxed<util::wrapped<T> (*)()> {
    typedef typename util::wrapped<T>::type type;
};

template<typename T>
struct unboxed<std::unique_ptr<util::guard<T>>> {
    typedef T type;
};

} } // namespace cppa::detail

#endif // CPPA_UNBOXED_HPP
