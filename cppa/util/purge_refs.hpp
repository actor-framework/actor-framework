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


#ifndef CPPA_PURGE_REFS_HPP
#define CPPA_PURGE_REFS_HPP

#include <functional>

#include "cppa/guard_expr.hpp"
#include "cppa/util/rm_ref.hpp"

namespace cppa { namespace util {

template<typename T>
struct purge_refs_impl {
    typedef T type;
};

template<typename T>
struct purge_refs_impl<ge_reference_wrapper<T> > {
    typedef T type;
};

template<typename T>
struct purge_refs_impl<ge_mutable_reference_wrapper<T> > {
    typedef T type;
};

template<typename T>
struct purge_refs_impl<std::reference_wrapper<T> > {
    typedef T type;
};

/**
 * @brief Removes references and reference wrappers.
 */
template<typename T>
struct purge_refs {
    typedef typename purge_refs_impl<typename util::rm_ref<T>::type>::type type;
};

} } // namespace cppa::util

#endif // CPPA_PURGE_REFS_HPP
