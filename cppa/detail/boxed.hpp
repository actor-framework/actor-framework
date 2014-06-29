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

#ifndef CPPA_DETAIL_BOXED_HPP
#define CPPA_DETAIL_BOXED_HPP

#include "cppa/anything.hpp"
#include "cppa/detail/wrapped.hpp"

namespace cppa {
namespace detail {

template<typename T>
struct boxed {
    typedef detail::wrapped<T> type;

};

template<typename T>
struct boxed<detail::wrapped<T>> {
    typedef detail::wrapped<T> type;

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
struct is_boxed<detail::wrapped<T>> {
    static constexpr bool value = true;

};

template<typename T>
struct is_boxed<detail::wrapped<T>()> {
    static constexpr bool value = true;

};

template<typename T>
struct is_boxed<detail::wrapped<T>(&)()> {
    static constexpr bool value = true;

};

template<typename T>
struct is_boxed<detail::wrapped<T>(*)()> {
    static constexpr bool value = true;

};

} // namespace detail
} // namespace cppa

#endif // CPPA_DETAIL_BOXED_HPP
