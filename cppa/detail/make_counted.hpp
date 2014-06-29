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

#ifndef CPPA_DETAIL_MAKE_COUNTED_HPP
#define CPPA_DETAIL_MAKE_COUNTED_HPP

#include "cppa/intrusive_ptr.hpp"

#include "cppa/ref_counted.hpp"

#include "cppa/mixin/memory_cached.hpp"

#include "cppa/detail/memory.hpp"

namespace cppa {
namespace detail {

template<typename T, typename... Ts>
typename std::enable_if<mixin::is_memory_cached<T>::value,
                        intrusive_ptr<T>>::type
make_counted(Ts&&... args) {
    return {detail::memory::create<T>(std::forward<Ts>(args)...)};
}

template<typename T, typename... Ts>
typename std::enable_if<!mixin::is_memory_cached<T>::value,
                        intrusive_ptr<T>>::type
make_counted(Ts&&... args) {
    return {new T(std::forward<Ts>(args)...)};
}

} // namespace detail
} // namespace cppa

#endif // CPPA_DETAIL_MAKE_COUNTED_HPP
