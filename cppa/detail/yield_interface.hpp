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


#ifndef CPPA_DETAIL_YIELD_INTERFACE_HPP
#define CPPA_DETAIL_YIELD_INTERFACE_HPP

#include <string>

namespace cppa {
namespace detail {

struct cs_thread;

enum class yield_state : int {
    // yield() wasn't called yet
    invalid,
    // actor is still ready
    ready,
    // actor waits for messages
    blocked,
    // actor finished execution
    done
};

// return to the scheduler / worker
void yield(yield_state);

// switches to @p what and returns to @p from after yield(...)
yield_state call(detail::cs_thread* what, detail::cs_thread* from);

} // namespace detail
} // namespace cppa

namespace cppa {

std::string to_string(detail::yield_state ys);

} // namespace cppa

#endif // CPPA_DETAIL_YIELD_INTERFACE_HPP
