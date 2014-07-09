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

#ifndef CPPA_PURGE_REFS_HPP
#define CPPA_PURGE_REFS_HPP

#include <functional>

#include "cppa/detail/type_traits.hpp"

namespace cppa {
namespace detail {

template<typename T>
struct purge_refs_impl {
    using type = T;

};

template<typename T>
struct purge_refs_impl<std::reference_wrapper<T>> {
    using type = T;

};

template<typename T>
struct purge_refs_impl<std::reference_wrapper<const T>> {
    using type = T;

};

/**
 * @brief Removes references and reference wrappers.
 */
template<typename T>
struct purge_refs {
    using type = typename purge_refs_impl<
                     typename detail::rm_const_and_ref<T>::type>::type;

};

} // namespace detail
} // namespace cppa

#endif // CPPA_PURGE_REFS_HPP
