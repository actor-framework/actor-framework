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


#ifndef CPPA_SCOPE_GUARD_HPP
#define CPPA_SCOPE_GUARD_HPP

#include <utility>

namespace cppa { namespace util {

/**
 * @brief A lightweight scope guard implementation.
 */
template<typename Fun>
class scope_guard {

    scope_guard() = delete;
    scope_guard(const scope_guard&) = delete;
    scope_guard& operator=(const scope_guard&) = delete;

 public:

    scope_guard(Fun f) : m_fun(std::move(f)), m_enabled(true) { }

    scope_guard(scope_guard&& other)
    : m_fun(std::move(other.m_fun)), m_enabled(other.m_enabled) {
        other.m_enabled = false;
    }

    ~scope_guard() {
        if (m_enabled) m_fun();
    }

    /**
     * @brief Disables this guard, i.e., the guard does not
     *        run its cleanup code as it goes out of scope.
     */
    inline void disable() {
        m_enabled = false;
    }

 private:

    Fun m_fun;
    bool m_enabled;

};

/**
 * @brief Creates a guard that executes @p f as soon as it
 *        goes out of scope.
 * @relates scope_guard
 */
template<typename Fun>
scope_guard<Fun> make_scope_guard(Fun f) {
    return {std::move(f)};
}

} } // namespace cppa::util

#endif // CPPA_SCOPE_GUARD_HPP
