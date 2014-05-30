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


#ifndef CPPA_MAY_HAVE_TIMEOUT_HPP
#define CPPA_MAY_HAVE_TIMEOUT_HPP

namespace cppa {

template<typename F>
struct timeout_definition;

class behavior;

template<typename T>
struct may_have_timeout {
    static constexpr bool value = false;
};

template<>
struct may_have_timeout<behavior> {
    static constexpr bool value = true;
};

template<typename F>
struct may_have_timeout<timeout_definition<F>> {
    static constexpr bool value = true;
};

} // namespace cppa

#endif // CPPA_MAY_HAVE_TIMEOUT_HPP
