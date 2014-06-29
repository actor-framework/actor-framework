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

#include "cppa/exit_reason.hpp"

namespace cppa {
namespace exit_reason {

static constexpr const char* s_names_table[] = {
    "not_exited",
    "normal",
    "unhandled_exception",
    "unallowed_function_call",
    "unhandled_sync_failure",
    "unhandled_sync_timeout"
};

const char* as_string(uint32_t value) {
    if (value <= unhandled_sync_timeout) return s_names_table[value];
    if (value == remote_link_unreachable) return "remote_link_unreachable";
    if (value >= user_defined) return "user_defined";
    return "illegal_exit_reason";
}

} // namespace exit_reason
} // namespace cppa
