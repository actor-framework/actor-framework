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
 * Copyright (C) 2011-2013                                                    *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation; either version 2.1 of the License,               *
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


#include <memory>

#include "cppa/detail/yield_interface.hpp"

namespace {

using namespace cppa;

__thread detail::yield_state* t_ystate = nullptr;
__thread util::fiber* t_caller = nullptr;
__thread util::fiber* t_callee = nullptr;

constexpr const char* names_table[] = {
    "yield_state::invalid",
    "yield_state::ready",
    "yield_state::blocked",
    "yield_state::done"
};

} // namespace <anonymous>

namespace cppa { namespace detail {

void yield(yield_state ystate) {
    *t_ystate = ystate;
    util::fiber::swap(*t_callee, *t_caller);
}

yield_state call(util::fiber* what, util::fiber* from) {
    yield_state result;
    t_ystate = &result;
    t_caller = from;
    t_callee = what;
    util::fiber::swap(*from, *what);
    return result;
}

} } // namespace cppa::detail

namespace cppa {

std::string to_string(detail::yield_state ys) {
    auto i = static_cast<size_t>(ys);
    return (i < sizeof(names_table)) ? names_table[i] : "{illegal yield_state}";
}

} // namespace cppa
