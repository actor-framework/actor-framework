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


#ifndef CPPA_UTIL_PURGE_REFS_HPP
#define CPPA_UTIL_PURGE_REFS_HPP

#include <functional>

#include "cppa/guard_expr.hpp"
#include "cppa/util/type_traits.hpp"
#include "cppa/util/rebindable_reference.hpp"

namespace cppa {
namespace util {

template<typename T>
struct purge_refs_impl {
    typedef T type;
};

template<typename T>
struct purge_refs_impl<util::rebindable_reference<T> > {
    typedef T type;
};

template<typename T>
struct purge_refs_impl<util::rebindable_reference<const T> > {
    typedef T type;
};

template<typename T>
struct purge_refs_impl<std::reference_wrapper<T> > {
    typedef T type;
};

template<typename T>
struct purge_refs_impl<std::reference_wrapper<const T> > {
    typedef T type;
};

/**
 * @brief Removes references and reference wrappers.
 */
template<typename T>
struct purge_refs {
    typedef typename purge_refs_impl<typename util::rm_const_and_ref<T>::type>::type type;
};

} // namespace util
} // namespace cppa


#endif // CPPA_UTIL_PURGE_REFS_HPP
