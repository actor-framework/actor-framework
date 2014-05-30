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


#include <memory>

#include "cppa/detail/cs_thread.hpp"
#include "cppa/detail/yield_interface.hpp"

namespace {

using namespace cppa;

__thread detail::yield_state* t_ystate = nullptr;
__thread detail::cs_thread* t_caller = nullptr;
__thread detail::cs_thread* t_callee = nullptr;

constexpr const char* names_table[] = {
    "yield_state::invalid",
    "yield_state::ready",
    "yield_state::blocked",
    "yield_state::done"
};

} // namespace <anonymous>

namespace cppa {
namespace detail {

void yield(yield_state ystate) {
    *t_ystate = ystate;
    detail::cs_thread::swap(*t_callee, *t_caller);
}

yield_state call(detail::cs_thread* what, detail::cs_thread* from) {
    yield_state result;
    t_ystate = &result;
    t_caller = from;
    t_callee = what;
    detail::cs_thread::swap(*from, *what);
    return result;
}

} // namespace util
} // namespace cppa


namespace cppa {

std::string to_string(detail::yield_state ys) {
    auto i = static_cast<size_t>(ys);
    return (i < sizeof(names_table)) ? names_table[i] : "{illegal yield_state}";
}

} // namespace cppa
