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


#ifndef CPPA_DETAIL_UNBOXED_HPP
#define CPPA_DETAIL_UNBOXED_HPP

#include <memory>

#include "cppa/util/guard.hpp"
#include "cppa/util/wrapped.hpp"

namespace cppa {
namespace detail {

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

} // namespace detail
} // namespace cppa

#endif // CPPA_DETAIL_UNBOXED_HPP
