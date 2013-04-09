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
 * Copyright (C) 2011-2013                                                    *
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


#ifndef CPPA_MIXED_HPP
#define CPPA_MIXED_HPP

// saves some typing
#define CPPA_MIXIN template<class,class> class

namespace cppa {

namespace detail {

template<class B, class D, CPPA_MIXIN... Ms>
struct extend_helper;

template<class B, class D>
struct extend_helper<B,D> { typedef D type; };

template<class B, class D, CPPA_MIXIN M, CPPA_MIXIN... Ms>
struct extend_helper<B,D,M,Ms...> : extend_helper<B,M<D,B>,Ms...> { };

} // namespace detail

/**
 * @brief Allows convenient definition of types using mixins.
 *        For example, @p extend<ar,T>::with<ob,fo> is an alias for
 *        @p fo<ob<ar,T>,T>.
 *
 * Mixins in libcppa always have two template parameters: base type and
 * derived type. This allows mixins to make use of the curiously recurring
 * template pattern (CRTP). However, if none of the used mixins use CRTP,
 * the second template argument can ignored (it is then set to Base).
 */
template<class Base, class Derived = Base>
struct extend {
    /**
     * @brief Identifies the combined type.
     */
    template<template<class,class> class... Mixins>
    using with = typename detail::extend_helper<Derived,Base,Mixins...>::type;
};

} // namespace cppa

#endif // CPPA_MIXED_HPP
